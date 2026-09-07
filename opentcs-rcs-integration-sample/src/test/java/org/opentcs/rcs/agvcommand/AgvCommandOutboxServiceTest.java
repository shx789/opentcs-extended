// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.agvcommand;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.util.List;
import java.util.Map;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.api.dto.Mission;
import org.opentcs.rcs.bridge.agv.MqttAgvRobotControlPublisher;

class AgvCommandOutboxServiceTest {

  @Test
  void shouldEnqueueMissionStartCommandIdempotently() {
    InMemoryAgvCommandOutboxStore store = new InMemoryAgvCommandOutboxStore();
    AgvCommandOutboxService service = new AgvCommandOutboxService(
        store,
        publisher()
    );
    Mission mission = mission();

    service.publishMissionStart(mission);
    service.publishMissionStart(mission);

    List<AgvCommandOutboxEntry> commands = store.findByMissionNo(mission.missionNo());
    assertThat(commands).hasSize(1);
    AgvCommandOutboxEntry command = commands.get(0);
    assertThat(command.commandStage()).isEqualTo(AgvCommandOutboxService.STAGE_MISSION_START);
    assertThat(command.idemKey()).isEqualTo("M1:MISSION_START");
    assertThat(command.status()).isEqualTo("PENDING");
    assertThat(command.retryCount()).isZero();
    assertThat(command.payloadJson()).contains("\"cmd_type\":\"interest_point_control\"");
    assertThat(command.payloadJson()).contains("\"id\":2");
  }

  private MqttAgvRobotControlPublisher publisher() {
    return new MqttAgvRobotControlPublisher(
        "tcp://127.0.0.1:1883",
        "test-client",
        "robot_control",
        1,
        null,
        null,
        Map.of("Point-02", 2),
        0.5,
        2,
        0,
        new ObjectMapper()
    );
  }

  private Mission mission() {
    return new Mission(
        "M1",
        "T1",
        "Point-01",
        "Point-02",
        "PLT001",
        50,
        "/api/wcs/agv/events"
    );
  }
}
