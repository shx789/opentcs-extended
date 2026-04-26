// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.ByteArrayInputStream;
import java.net.URI;
import java.net.http.HttpClient;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.time.Instant;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.api.wcs.WmsTaskResultService;
import org.opentcs.rcs.callback.CallbackOutboxService;
import org.opentcs.rcs.callback.CallbackRetryProcessor;
import org.opentcs.rcs.callback.InMemoryCallbackOutboxStore;
import org.opentcs.rcs.core.mission.InMemoryMissionStore;
import org.opentcs.rcs.core.mission.MissionCallbackTarget;
import org.opentcs.rcs.core.task.InMemoryTaskStore;

class OpenTcsSseTransportOrderSubscriberTest {

  @Test
  void shouldConsumeTransportOrderSseEventAndDispatchCallback() throws Exception {
    AtomicInteger callbackCalls = new AtomicInteger();
    TestFixture fixture = createFixture((url, payload) -> callbackCalls.incrementAndGet());
    fixture.missionStore.save(new MissionCallbackTarget("M1", "T1", "/api/v1/wcs/agv/events", "trace-1", "request-1"));
    OpenTcsSseTransportOrderSubscriber subscriber = fixture.subscriber(
        null,
        null,
        Duration.ofSeconds(1),
        Duration.ofMillis(100),
        Duration.ofSeconds(2)
    );

    String ssePayload = """
        id: 100
        event: /events/transportOrders
        data: {"eventTime":"2026-04-14T10:35:21Z","currentObjectState":{"name":"M1","state":"FINISHED","currentDriveOrderIndex":1,"processingVehicle":"AGV_01","properties":{"task_no":"T1"}}}

        """;
    subscriber.consumeEventStream(new ByteArrayInputStream(ssePayload.getBytes(StandardCharsets.UTF_8)));

    assertThat(callbackCalls.get()).isEqualTo(1);
    assertThat(subscriber.lastEventId()).isEqualTo("100");
    assertThat(fixture.outboxStore.findDue(Instant.now().plusSeconds(1), 10)).isEmpty();
  }

  @Test
  void shouldIncludeResumeAndAuthHeadersWhenBuildingSseRequest() throws Exception {
    TestFixture fixture = createFixture((url, payload) -> {
    });
    OpenTcsSseTransportOrderSubscriber subscriber = fixture.subscriber(
        "access-key",
        "bearer-token",
        Duration.ofSeconds(1),
        Duration.ofMillis(100),
        Duration.ofSeconds(2)
    );
    String ssePayload = """
        id: 42
        event: /events/vehicles
        data: {}

        """;
    subscriber.consumeEventStream(new ByteArrayInputStream(ssePayload.getBytes(StandardCharsets.UTF_8)));

    assertThat(subscriber.buildRequest().headers().firstValue("Last-Event-ID"))
        .contains("42");
    assertThat(subscriber.buildRequest().headers().firstValue("X-Api-Access-Key"))
        .contains("access-key");
    assertThat(subscriber.buildRequest().headers().firstValue("Authorization"))
        .contains("Bearer bearer-token");
  }

  @Test
  void shouldApplyReconnectDelayWithExponentialBackoffAndCap() {
    TestFixture fixture = createFixture((url, payload) -> {
    });
    OpenTcsSseTransportOrderSubscriber subscriber = fixture.subscriber(
        null,
        null,
        Duration.ofSeconds(1),
        Duration.ofMillis(1000),
        Duration.ofMillis(5000)
    );

    assertThat(subscriber.reconnectDelayMillisForAttempt(1)).isEqualTo(1000L);
    assertThat(subscriber.reconnectDelayMillisForAttempt(2)).isEqualTo(2000L);
    assertThat(subscriber.reconnectDelayMillisForAttempt(3)).isEqualTo(4000L);
    assertThat(subscriber.reconnectDelayMillisForAttempt(4)).isEqualTo(5000L);
    assertThat(subscriber.reconnectDelayMillisForAttempt(5)).isEqualTo(5000L);
  }

  private TestFixture createFixture(org.opentcs.rcs.callback.CallbackSender callbackSender) {
    InMemoryMissionStore missionStore = new InMemoryMissionStore();
    InMemoryTaskStore taskStore = new InMemoryTaskStore();
    InMemoryCallbackOutboxStore outboxStore = new InMemoryCallbackOutboxStore();
    ObjectMapper objectMapper = new ObjectMapper();
    CallbackOutboxService outboxService = new CallbackOutboxService(outboxStore, objectMapper);
    OpenTcsSseEventConsumer eventConsumer = new OpenTcsSseEventConsumer(
        new OpenTcsEventProjector(),
        outboxService,
        missionStore,
        taskStore,
        new WmsTaskResultService(outboxService, null)
    );
    CallbackRetryProcessor retryProcessor = new CallbackRetryProcessor(outboxStore, callbackSender);
    OpenTcsSsePayloadParser payloadParser = new OpenTcsSsePayloadParser(objectMapper);
    return new TestFixture(missionStore, outboxStore, eventConsumer, retryProcessor, payloadParser);
  }

  private record TestFixture(
      InMemoryMissionStore missionStore,
      InMemoryCallbackOutboxStore outboxStore,
      OpenTcsSseEventConsumer eventConsumer,
      CallbackRetryProcessor retryProcessor,
      OpenTcsSsePayloadParser payloadParser
  ) {

    private OpenTcsSseTransportOrderSubscriber subscriber(
        String apiAccessKey,
        String bearerToken,
        Duration requestTimeout,
        Duration reconnectInitialDelay,
        Duration reconnectMaxDelay
    ) {
      return new OpenTcsSseTransportOrderSubscriber(
          HttpClient.newHttpClient(),
          URI.create("http://127.0.0.1:55200/v1/sse?/events/transportOrders=true"),
          requestTimeout,
          reconnectInitialDelay,
          reconnectMaxDelay,
          apiAccessKey,
          bearerToken,
          payloadParser,
          eventConsumer,
          retryProcessor,
          100
      );
    }
  }
}
