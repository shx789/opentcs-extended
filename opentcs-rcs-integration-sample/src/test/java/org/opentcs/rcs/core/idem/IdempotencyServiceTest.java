// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.idem;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

import com.fasterxml.jackson.databind.ObjectMapper;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.api.dto.CreateMissionReq;
import org.opentcs.rcs.api.dto.CreateMissionResp;

class IdempotencyServiceTest {

  private IdempotencyService service;
  private ObjectMapper objectMapper;

  @BeforeEach
  void setUp() {
    objectMapper = new ObjectMapper();
    service = new IdempotencyService(new InMemoryIdempotencyStore(), objectMapper);
  }

  @Test
  void shouldHitAfterStore() {
    CreateMissionReq request = new CreateMissionReq(
        "M202602100001",
        "T202602090001",
        "P_WAIT_IN_01",
        "ST_IN_01",
        "PLT000000123",
        50,
        "/api/v1/wcs/agv/events"
    );
    CreateMissionResp response = new CreateMissionResp(
        request.missionNo(),
        request.taskNo(),
        "RECEIVED",
        false
    );

    IdempotencyResult first = service.check("MISSION_CREATE", request.missionNo(), request);
    assertThat(first.hit()).isFalse();

    service.storeSuccess("MISSION_CREATE", request.missionNo(), request, response);

    IdempotencyResult second = service.check("MISSION_CREATE", request.missionNo(), request);
    assertThat(second.hit()).isTrue();
    CreateMissionResp cached = second.cachedResponse(objectMapper, CreateMissionResp.class);
    assertThat(cached.missionNo()).isEqualTo(request.missionNo());
    assertThat(cached.taskNo()).isEqualTo(request.taskNo());
  }

  @Test
  void shouldRejectDifferentPayloadForSameIdemKey() {
    CreateMissionReq request = new CreateMissionReq(
        "M202602100001",
        "T202602090001",
        "P_WAIT_IN_01",
        "ST_IN_01",
        "PLT000000123",
        50,
        "/api/v1/wcs/agv/events"
    );
    service.storeSuccess(
        "MISSION_CREATE",
        request.missionNo(),
        request,
        new CreateMissionResp(request.missionNo(), request.taskNo(), "RECEIVED", false)
    );

    CreateMissionReq differentRequest = new CreateMissionReq(
        request.missionNo(),
        request.taskNo(),
        "P_WAIT_IN_02",
        "ST_IN_01",
        "PLT000000123",
        50,
        "/api/v1/wcs/agv/events"
    );

    assertThatThrownBy(
        () -> service.check("MISSION_CREATE", request.missionNo(), differentRequest)
    ).isInstanceOf(IllegalArgumentException.class);
  }
}
