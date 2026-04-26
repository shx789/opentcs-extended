// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;
import org.opentcs.rcs.callback.CallbackRetryProcessor;
import org.opentcs.rcs.http.RequestContext;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Subscribes to openTCS SSE transport-order events and forwards mapped callbacks.
 */
public class OpenTcsSseTransportOrderSubscriber {

  private static final Logger LOG = LoggerFactory.getLogger(OpenTcsSseTransportOrderSubscriber.class);
  private static final String TARGET_EVENT = "/events/transportOrders";

  private final HttpClient httpClient;
  private final URI sseUri;
  private final Duration requestTimeout;
  private final Duration reconnectInitialDelay;
  private final Duration reconnectMaxDelay;
  private final String apiAccessKey;
  private final String bearerToken;
  private final OpenTcsSsePayloadParser payloadParser;
  private final OpenTcsSseEventConsumer eventConsumer;
  private final CallbackRetryProcessor callbackRetryProcessor;
  private final int callbackDispatchBatchSize;

  private final AtomicBoolean running = new AtomicBoolean(false);
  private volatile String lastEventId;
  private volatile Thread workerThread;

  public OpenTcsSseTransportOrderSubscriber(
      HttpClient httpClient,
      URI sseUri,
      Duration requestTimeout,
      Duration reconnectInitialDelay,
      Duration reconnectMaxDelay,
      String apiAccessKey,
      String bearerToken,
      OpenTcsSsePayloadParser payloadParser,
      OpenTcsSseEventConsumer eventConsumer,
      CallbackRetryProcessor callbackRetryProcessor,
      int callbackDispatchBatchSize
  ) {
    this.httpClient = Objects.requireNonNull(httpClient, "httpClient");
    this.sseUri = Objects.requireNonNull(sseUri, "sseUri");
    this.requestTimeout = requirePositive(requestTimeout, "requestTimeout");
    this.reconnectInitialDelay = requirePositive(reconnectInitialDelay, "reconnectInitialDelay");
    this.reconnectMaxDelay = requirePositive(reconnectMaxDelay, "reconnectMaxDelay");
    if (reconnectInitialDelay.compareTo(reconnectMaxDelay) > 0) {
      throw new IllegalArgumentException("reconnectInitialDelay must not be greater than reconnectMaxDelay");
    }
    this.apiAccessKey = normalizeNullable(apiAccessKey);
    this.bearerToken = normalizeNullable(bearerToken);
    this.payloadParser = Objects.requireNonNull(payloadParser, "payloadParser");
    this.eventConsumer = Objects.requireNonNull(eventConsumer, "eventConsumer");
    this.callbackRetryProcessor = Objects.requireNonNull(callbackRetryProcessor, "callbackRetryProcessor");
    if (callbackDispatchBatchSize < 1) {
      throw new IllegalArgumentException("callbackDispatchBatchSize must be greater than 0");
    }
    this.callbackDispatchBatchSize = callbackDispatchBatchSize;
  }

  public synchronized void start() {
    if (running.get()) {
      return;
    }
    running.set(true);
    Thread thread = new Thread(this::runLoop, "opentcs-sse-subscriber");
    thread.setDaemon(true);
    workerThread = thread;
    thread.start();
  }

  public synchronized void stop() {
    running.set(false);
    Thread thread = workerThread;
    if (thread != null) {
      thread.interrupt();
    }
  }

  String lastEventId() {
    return lastEventId;
  }

  HttpRequest buildRequest() {
    HttpRequest.Builder requestBuilder = HttpRequest.newBuilder(sseUri)
        .timeout(requestTimeout)
        .header("Accept", "text/event-stream")
        .GET();
    if (lastEventId != null && !lastEventId.isBlank()) {
      requestBuilder.header("Last-Event-ID", lastEventId);
    }
    if (apiAccessKey != null) {
      requestBuilder.header("X-Api-Access-Key", apiAccessKey);
    }
    if (bearerToken != null) {
      requestBuilder.header("Authorization", "Bearer " + bearerToken);
    }
    return requestBuilder.build();
  }

  long reconnectDelayMillisForAttempt(int attempt) {
    long delay = reconnectInitialDelay.toMillis();
    for (int index = 1; index < attempt && delay < reconnectMaxDelay.toMillis(); index++) {
      delay = Math.min(delay * 2L, reconnectMaxDelay.toMillis());
    }
    return delay;
  }

  void consumeEventStream(InputStream inputStream) throws IOException {
    try (BufferedReader reader = new BufferedReader(
        new InputStreamReader(inputStream, StandardCharsets.UTF_8)
    )) {
      SseFrame frame = new SseFrame();
      String line;
      while ((line = reader.readLine()) != null) {
        if (line.isEmpty()) {
          dispatchFrame(frame);
          frame = new SseFrame();
          continue;
        }
        if (line.startsWith(":")) {
          continue;
        }
        int separator = line.indexOf(':');
        String field = separator >= 0 ? line.substring(0, separator) : line;
        String value = separator >= 0 ? stripSingleLeadingSpace(line.substring(separator + 1)) : "";
        switch (field) {
          case "id" -> frame.id = value;
          case "event" -> frame.event = value;
          case "data" -> frame.appendData(value);
          default -> {
          }
        }
      }
      dispatchFrame(frame);
    }
  }

  private void runLoop() {
    int failedAttempts = 0;
    while (running.get()) {
      try {
        consumeOnce();
        failedAttempts = 0;
      }
      catch (RuntimeException exc) {
        failedAttempts++;
        LOG.warn("openTCS SSE stream disconnected (attempt={}): {}", failedAttempts, exc.getMessage());
      }
      sleepBeforeReconnect(failedAttempts + 1);
    }
  }

  private void consumeOnce() {
    HttpRequest request = buildRequest();
    try {
      HttpResponse<InputStream> response = httpClient.send(
          request,
          HttpResponse.BodyHandlers.ofInputStream()
      );
      if (response.statusCode() < 200 || response.statusCode() >= 300) {
        throw new IllegalStateException("openTCS SSE request failed. status=" + response.statusCode());
      }
      consumeEventStream(response.body());
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not read openTCS SSE stream", exc);
    }
    catch (InterruptedException exc) {
      Thread.currentThread().interrupt();
      throw new IllegalStateException("Interrupted while consuming openTCS SSE stream", exc);
    }
  }

  private void dispatchFrame(SseFrame frame) {
    if (frame.id != null && !frame.id.isBlank()) {
      lastEventId = frame.id;
    }
    if (frame.event == null || frame.event.isBlank() || frame.data == null) {
      return;
    }
    String payloadJson = frame.data.toString();
    if (payloadJson.isBlank()) {
      return;
    }
    if (!TARGET_EVENT.equals(frame.event)) {
      return;
    }
    try {
      OpenTcsTransportOrderEvent event = payloadParser.parseTransportOrderEvent(payloadJson);
      eventConsumer.consume(event, RequestContext.generated());
      callbackRetryProcessor.processDue(callbackDispatchBatchSize);
    }
    catch (RuntimeException exc) {
      LOG.warn("Could not process openTCS transport-order SSE event: {}", exc.getMessage());
    }
  }

  private void sleepBeforeReconnect(int attempt) {
    if (!running.get()) {
      return;
    }
    long sleepMillis = reconnectDelayMillisForAttempt(attempt);
    try {
      Thread.sleep(sleepMillis);
    }
    catch (InterruptedException exc) {
      Thread.currentThread().interrupt();
    }
  }

  private static Duration requirePositive(Duration value, String fieldName) {
    Objects.requireNonNull(value, fieldName);
    if (value.isZero() || value.isNegative()) {
      throw new IllegalArgumentException(fieldName + " must be positive");
    }
    return value;
  }

  private static String normalizeNullable(String value) {
    if (value == null || value.isBlank()) {
      return null;
    }
    return value.trim();
  }

  private static String stripSingleLeadingSpace(String value) {
    if (value.startsWith(" ")) {
      return value.substring(1);
    }
    return value;
  }

  private static class SseFrame {

    private String id;
    private String event;
    private StringBuilder data;

    private void appendData(String line) {
      if (data == null) {
        data = new StringBuilder();
      }
      else {
        data.append('\n');
      }
      data.append(line);
    }
  }
}
