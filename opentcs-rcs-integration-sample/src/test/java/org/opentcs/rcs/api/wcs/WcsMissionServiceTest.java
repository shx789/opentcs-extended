// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.api.dto.CancelMissionResp;
import org.opentcs.rcs.api.dto.CreateMissionReq;
import org.opentcs.rcs.api.dto.CreateMissionResp;
import org.opentcs.rcs.api.dto.QueryMissionResp;
import org.opentcs.rcs.bridge.opentcs.OpenTcsOrderClient;
import org.opentcs.rcs.bridge.opentcs.OpenTcsPayloadMapper;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsTransportOrderReq;
import org.opentcs.rcs.core.idem.IdempotencyService;
import org.opentcs.rcs.core.idem.InMemoryIdempotencyStore;
import org.opentcs.rcs.core.mission.InMemoryMissionStore;
import org.opentcs.rcs.http.RequestContext;

class WcsMissionServiceTest {

  private static final RequestContext REQUEST_CONTEXT = new RequestContext("trace-1", "request-1");

  private AtomicInteger openTcsCalls;
  private AtomicInteger openTcsCancelCalls;
  private WcsMissionService service;

  @BeforeEach
  void setUp() {
    openTcsCalls = new AtomicInteger();
    openTcsCancelCalls = new AtomicInteger();
    service = new WcsMissionService(
        new IdempotencyService(new InMemoryIdempotencyStore(), new ObjectMapper()),
        new OpenTcsPayloadMapper(),
        new OpenTcsOrderClient() {
          @Override
          public void createTransportOrder(String orderName, OpenTcsTransportOrderReq payload) {
            openTcsCalls.incrementAndGet();
          }

          @Override
          public void cancelTransportOrder(String orderName) {
            openTcsCancelCalls.incrementAndGet();
          }
        },
        new ObjectMapper(),
        new InMemoryMissionStore()
    );
  }

  @Test
  void shouldCreateOrderOnFirstCallAndReturnIdemHitOnSecondCall() {
    CreateMissionReq req = new CreateMissionReq(
        "M202602100001",
        "T202602090001",
        "P_WAIT_IN_01",
        "ST_IN_01",
        "PLT000000123",
        30,
        "/api/v1/wcs/agv/events"
    );

    CreateMissionResp firstResp = service.createMission(req, REQUEST_CONTEXT);
    CreateMissionResp secondResp = service.createMission(req, REQUEST_CONTEXT);

    assertThat(firstResp.idemHit()).isFalse();
    assertThat(secondResp.idemHit()).isTrue();
    assertThat(openTcsCalls.get()).isEqualTo(1);
  }

  @Test
  void shouldCancelAndQueryMission() {
    CreateMissionReq req = new CreateMissionReq(
        "M202602100011",
        "T202602090011",
        "P_WAIT_IN_01",
        "ST_IN_01",
        "PLT000000123",
        30,
        "/api/v1/wcs/agv/events"
    );
    service.createMission(req, REQUEST_CONTEXT);

    QueryMissionResp beforeCancel = service.queryMission(req.missionNo());
    assertThat(beforeCancel.rcsStatus()).isEqualTo("RECEIVED");

    CancelMissionResp cancelResp = service.cancelMission(req.missionNo());
    assertThat(cancelResp.rcsStatus()).isEqualTo("CANCELED");
    assertThat(cancelResp.idemHit()).isFalse();
    assertThat(openTcsCancelCalls.get()).isEqualTo(1);

    CancelMissionResp secondCancelResp = service.cancelMission(req.missionNo());
    assertThat(secondCancelResp.idemHit()).isTrue();
    assertThat(openTcsCancelCalls.get()).isEqualTo(1);

    QueryMissionResp afterCancel = service.queryMission(req.missionNo());
    assertThat(afterCancel.rcsStatus()).isEqualTo("CANCELED");
  }
}
