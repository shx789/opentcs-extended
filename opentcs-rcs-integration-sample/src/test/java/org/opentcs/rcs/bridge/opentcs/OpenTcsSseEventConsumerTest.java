// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.time.Instant;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.api.dto.AgvEventCallbackReq;
import org.opentcs.rcs.api.wcs.WmsTaskResultService;
import org.opentcs.rcs.callback.CallbackOutboxService;
import org.opentcs.rcs.callback.InMemoryCallbackOutboxStore;
import org.opentcs.rcs.core.mission.InMemoryMissionStore;
import org.opentcs.rcs.core.mission.MissionCallbackTarget;
import org.opentcs.rcs.core.task.InMemoryTaskStore;
import org.opentcs.rcs.core.task.WcsTaskRecord;
import org.opentcs.rcs.core.task.WcsTaskStatus;
import org.opentcs.rcs.core.task.WcsTaskType;
import org.opentcs.rcs.http.RequestContext;

class OpenTcsSseEventConsumerTest {

  @Test
  void shouldEnqueueMappedCallbackEvent() {
    InMemoryCallbackOutboxStore store = new InMemoryCallbackOutboxStore();
    InMemoryMissionStore missionStore = new InMemoryMissionStore();
    InMemoryTaskStore taskStore = new InMemoryTaskStore();
    missionStore.save(
        new MissionCallbackTarget("M1", "T1", "/api/v1/wcs/agv/events", "trace-m1", "request-m1")
    );
    OpenTcsSseEventConsumer consumer = new OpenTcsSseEventConsumer(
        new OpenTcsEventProjector(),
        new CallbackOutboxService(store, new ObjectMapper()),
        missionStore,
        taskStore,
        new WmsTaskResultService(new CallbackOutboxService(store, new ObjectMapper()), null)
    );
    OpenTcsTransportOrderEvent event = new OpenTcsTransportOrderEvent(
        "M1",
        "T1",
        "FINISHED",
        1,
        "Point-0020",
        "AGV_01",
        null,
        Instant.parse("2026-02-10T10:35:21Z")
    );

    AgvEventCallbackReq mapped = consumer.consume(
        event,
        new RequestContext("trace-evt", "request-evt")
    ).orElseThrow();

    assertThat(mapped.eventType()).isEqualTo("DROPPED");
    assertThat(store.findDue(Instant.now().plusSeconds(1), 10)).hasSize(1);
    assertThat(store.findDue(Instant.now().plusSeconds(1), 10).get(0).callbackUrl())
        .isEqualTo("/api/v1/wcs/agv/events");
    assertThat(missionStore.findByMissionNo("M1").orElseThrow().rcsStatus()).isEqualTo("DONE");
  }

  @Test
  void shouldProjectOutboundTaskDoneAndEnqueueWmsResult() {
    InMemoryCallbackOutboxStore store = new InMemoryCallbackOutboxStore();
    ObjectMapper objectMapper = new ObjectMapper();
    CallbackOutboxService callbackOutboxService = new CallbackOutboxService(store, objectMapper);
    InMemoryMissionStore missionStore = new InMemoryMissionStore();
    InMemoryTaskStore taskStore = new InMemoryTaskStore();
    missionStore.save(
        new MissionCallbackTarget("M2", "T2", "/api/v1/wcs/agv/events", "trace-m2", "request-m2")
    );
    taskStore.save(
        new WcsTaskRecord(
            "BIZ-OUT-002",
            WcsTaskType.OUTBOUND,
            "M2",
            "T2",
            null,
            WcsTaskStatus.RECEIVED,
            "Point-0026",
            "Point-0020",
            "PLT002",
            60,
            "/api/v1/wcs/agv/events",
            "trace-m2",
            "request-m2",
            Instant.now().toString(),
            Instant.now().toString()
        )
    );
    OpenTcsSseEventConsumer consumer = new OpenTcsSseEventConsumer(
        new OpenTcsEventProjector(),
        callbackOutboxService,
        missionStore,
        taskStore,
        new WmsTaskResultService(callbackOutboxService, "http://127.0.0.1:18081")
    );
    OpenTcsTransportOrderEvent event = new OpenTcsTransportOrderEvent(
        "M2",
        "T2",
        "FINISHED",
        1,
        "Point-0020",
        "AGV_01",
        null,
        Instant.parse("2026-02-10T10:35:21Z")
    );

    consumer.consume(event, new RequestContext("trace-evt", "request-evt"));

    assertThat(taskStore.findByBizTaskNo("BIZ-OUT-002").orElseThrow().rcsStatus()).isEqualTo(
        WcsTaskStatus.DONE
    );
    assertThat(store.findDue(Instant.now().plusSeconds(1), 10)).hasSize(2);
    assertThat(store.findDue(Instant.now().plusSeconds(1), 10))
        .anyMatch(
            entry -> "http://127.0.0.1:18081/api/v1/wms/outbound-results".equals(
                entry.callbackUrl()
            )
                && "BIZ-OUT-002|DONE".equals(entry.idemKey())
        );
  }
}
