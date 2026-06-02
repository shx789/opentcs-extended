// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.time.Instant;
import org.junit.jupiter.api.Test;

class AgvMqttStatusPayloadParserTest {

  @Test
  void shouldParseSnakeCasePayload() {
    AgvMqttStatusPayloadParser parser = new AgvMqttStatusPayloadParser(new ObjectMapper());

    AgvMqttStatusMessage message = parser.parse(
        """
        {
          "message_id": "MSG-1",
          "agv_id": "AGV_01",
          "event_type": "dropped",
          "mission_no": "M1",
          "task_no": "T1",
          "point_id": "ST_OUT_01",
          "battery": 88,
          "seq": 12,
          "event_time": "2026-02-10T10:35:21Z"
        }
        """
    );

    assertThat(message.messageId()).isEqualTo("MSG-1");
    assertThat(message.agvId()).isEqualTo("AGV_01");
    assertThat(message.eventType()).isEqualTo("DROPPED");
    assertThat(message.missionNo()).isEqualTo("M1");
    assertThat(message.battery()).isEqualTo(88);
    assertThat(message.sequence()).isEqualTo(12L);
    assertThat(message.eventTime()).isEqualTo(Instant.parse("2026-02-10T10:35:21Z"));
  }
}
