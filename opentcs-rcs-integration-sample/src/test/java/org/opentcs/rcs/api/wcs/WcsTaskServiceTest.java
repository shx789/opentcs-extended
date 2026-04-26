// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.api.dto.CreateWcsTaskReq;
import org.opentcs.rcs.api.dto.CreateWcsTaskResp;
import org.opentcs.rcs.api.dto.QueryWcsTaskResp;
import org.opentcs.rcs.bridge.opentcs.OpenTcsOrderClient;
import org.opentcs.rcs.bridge.opentcs.OpenTcsPayloadMapper;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsTransportOrderReq;
import org.opentcs.rcs.core.idem.IdempotencyService;
import org.opentcs.rcs.core.idem.InMemoryIdempotencyStore;
import org.opentcs.rcs.core.mission.InMemoryMissionStore;
import org.opentcs.rcs.core.mission.MissionStore;
import org.opentcs.rcs.core.task.InMemoryTaskStore;
import org.opentcs.rcs.core.task.TaskStore;
import org.opentcs.rcs.http.RequestContext;

class WcsTaskServiceTest {

  private static final RequestContext REQUEST_CONTEXT = new RequestContext("trace-1", "request-1");

  private AtomicInteger createCalls;
  private MissionStore missionStore;
  private WcsTaskService service;

  @BeforeEach
  void setUp() {
    createCalls = new AtomicInteger();
    missionStore = new InMemoryMissionStore();
    TaskStore taskStore = new InMemoryTaskStore();
    ObjectMapper objectMapper = new ObjectMapper();
    WcsMissionService missionService = new WcsMissionService(
        new IdempotencyService(new InMemoryIdempotencyStore(), objectMapper),
        new OpenTcsPayloadMapper(),
        new OpenTcsOrderClient() {
          @Override
          public void createTransportOrder(String orderName, OpenTcsTransportOrderReq payload) {
            createCalls.incrementAndGet();
          }

          @Override
          public void cancelTransportOrder(String orderName) {
          }
        },
        objectMapper,
        missionStore
    );
    service = new WcsTaskService(
        new IdempotencyService(new InMemoryIdempotencyStore(), objectMapper),
        missionService,
        taskStore,
        objectMapper
    );
  }

  @Test
  void shouldCreateInboundTaskWithIdempotency() {
    CreateWcsTaskReq req = new CreateWcsTaskReq(
        "BIZ-IN-001",
        null,
        null,
        "Point-0020",
        "Point-0026",
        "PLT0001",
        80,
        "/api/v1/wcs/agv/events"
    );

    CreateWcsTaskResp first = service.createInboundTask(req, REQUEST_CONTEXT);
    CreateWcsTaskResp second = service.createInboundTask(req, REQUEST_CONTEXT);

    assertThat(first.idemHit()).isFalse();
    assertThat(second.idemHit()).isTrue();
    assertThat(first.missionNo()).isEqualTo("M_BIZ-IN-001");
    assertThat(first.taskNo()).isEqualTo("T_BIZ-IN-001");
    assertThat(createCalls.get()).isEqualTo(1);
  }

  @Test
  void shouldMapMissionDoneToWaitPlcForInboundTask() {
    CreateWcsTaskReq req = new CreateWcsTaskReq(
        "BIZ-IN-002",
        "M_IN_002",
        "T_IN_002",
        "Point-0020",
        "Point-0026",
        "PLT0002",
        60,
        "/api/v1/wcs/agv/events"
    );
    service.createInboundTask(req, REQUEST_CONTEXT);
    missionStore.updateStatus("M_IN_002", "DONE");

    QueryWcsTaskResp queryResp = service.queryTask("BIZ-IN-002");

    assertThat(queryResp.rcsStatus()).isEqualTo("WAIT_PLC");
  }

  @Test
  void shouldRejectCancelOnTerminalTask() {
    CreateWcsTaskReq req = new CreateWcsTaskReq(
        "BIZ-OUT-001",
        "M_OUT_001",
        "T_OUT_001",
        "Point-0026",
        "Point-0020",
        "PLT0003",
        50,
        "/api/v1/wcs/agv/events"
    );
    service.createOutboundTask(req, REQUEST_CONTEXT);
    missionStore.updateStatus("M_OUT_001", "FAILED");
    service.queryTask("BIZ-OUT-001");

    assertThatThrownBy(() -> service.cancelTask("BIZ-OUT-001"))
        .isInstanceOf(TaskStateConflictException.class)
        .hasMessageContaining("terminal status");
  }
}
