// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.util.Map;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.api.dto.Mission;

class MqttAgvRobotControlPublisherTest {

  @Test
  void shouldBuildInterestPointStartPayloadForMissionTarget() {
    MqttAgvRobotControlPublisher publisher = new MqttAgvRobotControlPublisher(
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

    Map<String, Object> payload = publisher.buildMissionStartPayload(
        new Mission(
            "M1",
            "T1",
            "Point-01",
            "Point-02",
            "PLT001",
            50,
            "/api/wcs/agv/events"
        )
    );

    assertThat(payload).containsEntry("cmd_type", "interest_point_control");
    assertThat(payload).containsEntry("cmd", "start");
    assertThat(payload).containsEntry("id", 2);
    assertThat(payload).containsEntry("run_speed", 0.5);
    assertThat(payload).containsEntry("path_stop_time", 0);
    assertThat(payload).containsEntry("path_mode", 2);
    assertThat(payload).containsEntry("circulates", 0);
    assertThat(payload).containsEntry("time", 0);
  }

  @Test
  void shouldRejectMissionWhenTargetPointMappingIsMissing() {
    MqttAgvRobotControlPublisher publisher = new MqttAgvRobotControlPublisher(
        "tcp://127.0.0.1:1883",
        "test-client",
        "robot_control",
        1,
        null,
        null,
        Map.of("Point-01", 1),
        0.5,
        2,
        0,
        new ObjectMapper()
    );

    assertThatThrownBy(
        () -> publisher.buildMissionStartPayload(
            new Mission(
                "M1",
                "T1",
                "Point-01",
                "Point-02",
                "PLT001",
                50,
                "/api/wcs/agv/events"
            )
        )
    ).isInstanceOf(IllegalArgumentException.class)
        .hasMessageContaining("No AGV point id mapping for to_point: Point-02");
  }

}
