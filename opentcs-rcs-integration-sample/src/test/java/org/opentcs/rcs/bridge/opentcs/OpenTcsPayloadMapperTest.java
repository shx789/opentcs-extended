// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import static org.assertj.core.api.Assertions.assertThat;

import java.time.Instant;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.api.dto.Mission;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsTransportOrderReq;

class OpenTcsPayloadMapperTest {

  @Test
  void shouldMapMissionToOpenTcsPayload() {
    OpenTcsPayloadMapper mapper = new OpenTcsPayloadMapper();
    Mission mission = new Mission(
        "M202602100001",
        "T202602090001",
        "P_WAIT_IN_01",
        "ST_IN_01",
        "PLT000000123",
        50,
        "/api/v1/wcs/agv/events"
    );

    OpenTcsTransportOrderReq payload = mapper.toTransportOrderReq(mission);

    assertThat(payload.destinations()).hasSize(2);
    assertThat(payload.destinations().get(0).locationName()).isEqualTo("LOC_P_WAIT_IN_01");
    assertThat(payload.destinations().get(0).operation()).isEqualTo("PICK");
    assertThat(payload.destinations().get(1).locationName()).isEqualTo("LOC_ST_IN_01");
    assertThat(payload.destinations().get(1).operation()).isEqualTo("DROP");
    assertThat(payload.properties())
        .anySatisfy(property -> assertThat(property.key()).isEqualTo("mission_no"));
  }

  @Test
  void shouldMapMoveOnlyMissionToMoveOperations() {
    OpenTcsPayloadMapper mapper = new OpenTcsPayloadMapper();
    Mission mission = new Mission(
        "M202602100002",
        "T202602090002",
        "P_WAIT_IN_01",
        "ST_IN_01",
        "PLT000000123",
        50,
        "/api/v1/wcs/agv/events",
        "MOVE_ONLY",
        null,
        null
    );

    OpenTcsTransportOrderReq payload = mapper.toTransportOrderReq(mission);

    assertThat(payload.destinations()).hasSize(2);
    assertThat(payload.destinations().get(0).operation()).isEqualTo("MOVE");
    assertThat(payload.destinations().get(1).operation()).isEqualTo("MOVE");
  }

  @Test
  void shouldPreferExplicitMissionOperations() {
    OpenTcsPayloadMapper mapper = new OpenTcsPayloadMapper();
    Mission mission = new Mission(
        "M202602100003",
        "T202602090003",
        "P_WAIT_IN_01",
        "ST_IN_01",
        "PLT000000123",
        50,
        "/api/v1/wcs/agv/events",
        "MOVE_ONLY",
        "WAIT",
        "CHARGE"
    );

    OpenTcsTransportOrderReq payload = mapper.toTransportOrderReq(mission);

    assertThat(payload.destinations()).hasSize(2);
    assertThat(payload.destinations().get(0).operation()).isEqualTo("WAIT");
    assertThat(payload.destinations().get(1).operation()).isEqualTo("CHARGE");
  }

  @Test
  void shouldGenerateEarlierDeadlineForHigherPriority() {
    OpenTcsPayloadMapper mapper = new OpenTcsPayloadMapper();
    Instant highPriorityDeadline = mapper.deadlineFromPriority(100);
    Instant lowPriorityDeadline = mapper.deadlineFromPriority(1);

    assertThat(highPriorityDeadline).isBefore(lowPriorityDeadline);
  }
}
