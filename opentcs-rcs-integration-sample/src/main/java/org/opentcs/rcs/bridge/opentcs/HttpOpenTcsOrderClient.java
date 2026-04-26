// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Objects;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsTransportOrderReq;

/**
 * HTTP-based openTCS order client with timeout, retry and bearer authentication support.
 */
public class HttpOpenTcsOrderClient
    implements OpenTcsOrderClient {

  private static final long MAX_RETRY_DELAY_MILLIS = 60_000L;

  private final HttpClient httpClient;
  private final ObjectMapper objectMapper;
  private final URI baseUri;
  private final Duration requestTimeout;
  private final int maxAttempts;
  private final Duration initialRetryDelay;
  private final String bearerToken;

  public HttpOpenTcsOrderClient(
      HttpClient httpClient,
      ObjectMapper objectMapper,
      URI baseUri,
      Duration requestTimeout,
      int maxAttempts,
      Duration initialRetryDelay,
      String bearerToken
  ) {
    this.httpClient = Objects.requireNonNull(httpClient, "httpClient");
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
    this.baseUri = Objects.requireNonNull(baseUri, "baseUri");
    this.requestTimeout = requirePositive(requestTimeout, "requestTimeout");
    this.maxAttempts = requirePositive(maxAttempts, "maxAttempts");
    this.initialRetryDelay = requireNotNegative(initialRetryDelay, "initialRetryDelay");
    this.bearerToken = normalizeToken(bearerToken);
  }

  @Override
  public void createTransportOrder(String orderName, OpenTcsTransportOrderReq payload) {
    Objects.requireNonNull(payload, "payload");
    requireNonBlank(orderName, "orderName");

    URI requestUri = buildCreateOrderUri(orderName);
    String payloadJson = toJson(payload);

    sendWithRetry(
        () -> buildRequest(requestUri, payloadJson),
        "openTCS create order failed",
        "Interrupted while creating openTCS order"
    );
  }

  @Override
  public void cancelTransportOrder(String orderName) {
    requireNonBlank(orderName, "orderName");
    URI requestUri = buildCancelOrderUri(orderName);
    sendWithRetry(
        () -> buildWithdrawalRequest(requestUri),
        "openTCS cancel order failed",
        "Interrupted while cancelling openTCS order"
    );
  }

  private HttpRequest buildRequest(URI requestUri, String payloadJson) {
    HttpRequest.Builder builder = HttpRequest.newBuilder(requestUri)
        .timeout(requestTimeout)
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .POST(HttpRequest.BodyPublishers.ofString(payloadJson));
    if (bearerToken != null) {
      builder.header("Authorization", "Bearer " + bearerToken);
    }
    return builder.build();
  }

  private URI buildCreateOrderUri(String orderName) {
    String base = baseUri.toString().endsWith("/") ? baseUri.toString() : baseUri + "/";
    String encodedOrderName = URLEncoder.encode(orderName, StandardCharsets.UTF_8)
        .replace("+", "%20");
    return URI.create(base + "v1/transportOrders/" + encodedOrderName);
  }

  private URI buildCancelOrderUri(String orderName) {
    String base = baseUri.toString().endsWith("/") ? baseUri.toString() : baseUri + "/";
    String encodedOrderName = URLEncoder.encode(orderName, StandardCharsets.UTF_8)
        .replace("+", "%20");
    return URI.create(base + "v1/transportOrders/" + encodedOrderName + "/withdrawal?immediate=true");
  }

  private HttpRequest buildWithdrawalRequest(URI requestUri) {
    HttpRequest.Builder builder = HttpRequest.newBuilder(requestUri)
        .timeout(requestTimeout)
        .header("Accept", "application/json")
        .POST(HttpRequest.BodyPublishers.noBody());
    if (bearerToken != null) {
      builder.header("Authorization", "Bearer " + bearerToken);
    }
    return builder.build();
  }

  private void sendWithRetry(
      RequestSupplier requestSupplier,
      String failureMessage,
      String interruptedMessage
  ) {
    for (int attempt = 1; attempt <= maxAttempts; attempt++) {
      try {
        HttpResponse<String> response = httpClient.send(
            requestSupplier.get(),
            HttpResponse.BodyHandlers.ofString()
        );
        int statusCode = response.statusCode();
        if (statusCode >= 200 && statusCode < 300) {
          return;
        }
        if (isRetryableStatus(statusCode) && attempt < maxAttempts) {
          waitBeforeRetry(attempt);
          continue;
        }
        throw new OpenTcsClientException(
            failureMessage + ". status=" + statusCode + ", body=" + safeBody(response.body())
        );
      }
      catch (IOException exc) {
        if (attempt >= maxAttempts) {
          throw new OpenTcsClientException(failureMessage + " after retries", exc);
        }
        waitBeforeRetry(attempt);
      }
      catch (InterruptedException exc) {
        Thread.currentThread().interrupt();
        throw new OpenTcsClientException(interruptedMessage, exc);
      }
    }
  }

  private String toJson(OpenTcsTransportOrderReq payload) {
    try {
      return objectMapper.writeValueAsString(payload);
    }
    catch (JsonProcessingException exc) {
      throw new IllegalStateException("Could not serialize openTCS payload", exc);
    }
  }

  private boolean isRetryableStatus(int statusCode) {
    return statusCode == 429 || statusCode >= 500;
  }

  private void waitBeforeRetry(int attempt) {
    long delayMillis = calculateDelayMillis(attempt);
    if (delayMillis <= 0L) {
      return;
    }
    try {
      Thread.sleep(delayMillis);
    }
    catch (InterruptedException exc) {
      Thread.currentThread().interrupt();
      throw new IllegalStateException("Interrupted while waiting for retry", exc);
    }
  }

  private long calculateDelayMillis(int attempt) {
    long delayMillis = initialRetryDelay.toMillis();
    if (delayMillis <= 0L) {
      return 0L;
    }
    for (int index = 1; index < attempt && delayMillis < MAX_RETRY_DELAY_MILLIS; index++) {
      delayMillis = Math.min(delayMillis * 2, MAX_RETRY_DELAY_MILLIS);
    }
    return delayMillis;
  }

  private String safeBody(String body) {
    if (body == null || body.isBlank()) {
      return "<empty>";
    }
    return body;
  }

  private static int requirePositive(int value, String fieldName) {
    if (value < 1) {
      throw new IllegalArgumentException(fieldName + " must be greater than 0");
    }
    return value;
  }

  private static Duration requirePositive(Duration value, String fieldName) {
    Objects.requireNonNull(value, fieldName);
    if (value.isZero() || value.isNegative()) {
      throw new IllegalArgumentException(fieldName + " must be positive");
    }
    return value;
  }

  private static Duration requireNotNegative(Duration value, String fieldName) {
    Objects.requireNonNull(value, fieldName);
    if (value.isNegative()) {
      throw new IllegalArgumentException(fieldName + " must not be negative");
    }
    return value;
  }

  private static void requireNonBlank(String value, String fieldName) {
    if (value == null || value.isBlank()) {
      throw new IllegalArgumentException(fieldName + " must not be blank");
    }
  }

  private static String normalizeToken(String token) {
    if (token == null || token.isBlank()) {
      return null;
    }
    return token.trim();
  }

  @FunctionalInterface
  private interface RequestSupplier {

    HttpRequest get();
  }
}
