// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.time.Instant;
import java.util.List;
import java.util.Map;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.api.dto.AgvEventCallbackReq;
import org.opentcs.rcs.api.wcs.WmsTaskResultService;
import org.opentcs.rcs.bridge.agv.mapping.AgvPointMappingStore;
import org.opentcs.rcs.bridge.opentcs.NoopOpenTcsVehiclePositionClient;
import org.opentcs.rcs.callback.CallbackOutboxService;
import org.opentcs.rcs.callback.InMemoryCallbackOutboxStore;
import org.opentcs.rcs.core.mission.InMemoryMissionStore;
import org.opentcs.rcs.core.mission.MissionCallbackTarget;
import org.opentcs.rcs.core.task.InMemoryTaskStore;
import org.opentcs.rcs.core.task.WcsTaskRecord;
import org.opentcs.rcs.core.task.WcsTaskStatus;
import org.opentcs.rcs.core.task.WcsTaskType;
import org.opentcs.rcs.http.RequestContext;

class AgvMqttStatusEventConsumerTest {

  @Test
  void shouldUpdateMissionAndEnqueueCallbackForDroppedEvent() {
    InMemoryCallbackOutboxStore store = new InMemoryCallbackOutboxStore();
    ObjectMapper objectMapper = new ObjectMapper();
    CallbackOutboxService callbackOutboxService = new CallbackOutboxService(store, objectMapper);
    InMemoryMissionStore missionStore = new InMemoryMissionStore();
    InMemoryTaskStore taskStore = new InMemoryTaskStore();
    missionStore.save(
        new MissionCallbackTarget(
            "M1",
            "T1",
            "/api/wcs/agv/events",
            "trace-1",
            "request-1"
        )
    );
    taskStore.save(
        new WcsTaskRecord(
            "BIZ-001",
            WcsTaskType.OUTBOUND,
            "M1",
            "T1",
            null,
            WcsTaskStatus.IN_PROGRESS,
            "P_WAIT_OUT_01",
            "ST_OUT_01",
            "PLT001",
            50,
            "/api/wcs/agv/events",
            "trace-1",
            "request-1",
            Instant.now().toString(),
            Instant.now().toString()
        )
    );
    AgvMqttStatusEventConsumer consumer = new AgvMqttStatusEventConsumer(
        callbackOutboxService,
        missionStore,
        taskStore,
        new WmsTaskResultService(callbackOutboxService, null),
        noopSynchronizer()
    );
    AgvMqttStatusMessage message = new AgvMqttStatusMessage(
        "MSG-1",
        "AGV_01",
        "DROPPED",
        "M1",
        "T1",
        "ST_OUT_01",
        87,
        null,
        null,
        null,
        null,
        null,
        null,
        1L,
        Instant.parse("2026-02-10T10:35:21Z")
    );

    AgvEventCallbackReq callback = consumer.consume(
        message,
        new RequestContext("trace-evt", "request-evt")
    ).orElseThrow();

    assertThat(callback.eventType()).isEqualTo("DROPPED");
    assertThat(callback.agvId()).isEqualTo("AGV_01");
    assertThat(callback.battery()).isEqualTo(87);
    assertThat(missionStore.findByMissionNo("M1").orElseThrow().rcsStatus()).isEqualTo("DONE");
    assertThat(taskStore.findByBizTaskNo("BIZ-001").orElseThrow().rcsStatus())
        .isEqualTo(WcsTaskStatus.DONE);
    assertThat(store.findDue(Instant.now().plusSeconds(1), 10))
        .anyMatch(entry -> "MSG-1".equals(entry.idemKey()));
  }

  @Test
  void shouldDeriveArrivedFromMissionPointsWithoutTaskRecord() {
    InMemoryCallbackOutboxStore store = new InMemoryCallbackOutboxStore();
    ObjectMapper objectMapper = new ObjectMapper();
    CallbackOutboxService callbackOutboxService = new CallbackOutboxService(store, objectMapper);
    InMemoryMissionStore missionStore = new InMemoryMissionStore();
    missionStore.save(
        new MissionCallbackTarget(
            "M2",
            "T2",
            "/api/wcs/agv/events",
            "trace-2",
            "request-2",
            "P_WAIT_IN_01",
            "ST_IN_01",
            "PLT002",
            50
        )
    );
    AgvMqttStatusEventConsumer consumer = new AgvMqttStatusEventConsumer(
        callbackOutboxService,
        missionStore,
        new InMemoryTaskStore(),
        new WmsTaskResultService(callbackOutboxService, null),
        noopSynchronizer()
    );

    AgvEventCallbackReq callback = consumer.consume(
        new AgvMqttStatusMessage(
            "MSG-ARRIVED",
            "AGV_01",
            "ARRIVED",
            "M2",
            "T2",
            "P_WAIT_IN_01",
            76,
            null,
            null,
            null,
            null,
            null,
            null,
            3L,
            Instant.parse("2026-02-10T10:36:21Z")
        ),
        RequestContext.generated()
    ).orElseThrow();

    assertThat(callback.eventType()).isEqualTo("ARRIVED_FROM");
    assertThat(missionStore.findByMissionNo("M2").orElseThrow().rcsStatus())
        .isEqualTo("IN_PROGRESS");
    assertThat(store.findDue(Instant.now().plusSeconds(1), 10))
        .anyMatch(entry -> "MSG-ARRIVED".equals(entry.idemKey()));
  }

  @Test
  void shouldIgnoreTelemetryOnlyEventForMissionCallbacks() {
    InMemoryCallbackOutboxStore store = new InMemoryCallbackOutboxStore();
    CallbackOutboxService callbackOutboxService = new CallbackOutboxService(
        store,
        new ObjectMapper()
    );
    AgvMqttStatusEventConsumer consumer = new AgvMqttStatusEventConsumer(
        callbackOutboxService,
        new InMemoryMissionStore(),
        new InMemoryTaskStore(),
        new WmsTaskResultService(callbackOutboxService, null),
        noopSynchronizer()
    );

    assertThat(
        consumer.consume(
            new AgvMqttStatusMessage(
                "MSG-2",
                "AGV_01",
                "BATTERY",
                null,
                null,
                null,
                76,
                null,
                null,
                null,
                null,
                null,
                null,
                2L,
                Instant.parse("2026-02-10T10:35:21Z")
            ),
            RequestContext.generated()
        )
    ).isEmpty();
  }

  private static AgvVehiclePositionSynchronizer noopSynchronizer() {
    return new AgvVehiclePositionSynchronizer(
        new NoopOpenTcsVehiclePositionClient(),
        new AgvPointMappingStore(List.of(), 0.5),
        Map.of("AGV_01", "Vehicle-01")
    );
  }

}
