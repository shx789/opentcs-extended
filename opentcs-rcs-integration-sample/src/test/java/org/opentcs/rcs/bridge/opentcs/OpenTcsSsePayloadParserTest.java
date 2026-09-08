// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import org.junit.jupiter.api.Test;

class OpenTcsSsePayloadParserTest {

  @Test
  void shouldParseCurrentObjectStatePayload() {
    String payload = """
        {
          "eventTime": "2026-02-10T10:35:21Z",
          "currentObjectState": {
            "name": "M202602100001",
            "state": "BEING_PROCESSED",
            "currentDriveOrderIndex": 0,
            "driveOrders": [
              {
                "destination": {
                  "destination": "Point-0001",
                  "operation": "MOVE",
                  "properties": {}
                },
                "state": "FINISHED"
              },
              {
                "destination": {
                  "destination": "Point-0003",
                  "operation": "MOVE",
                  "properties": {}
                },
                "state": "TRAVELLING"
              }
            ],
            "processingVehicle": "AGV_01",
            "properties": {
              "task_no": "T202602090001"
            }
          },
          "previousObjectState": null
        }
        """;
    OpenTcsSsePayloadParser parser = new OpenTcsSsePayloadParser(new ObjectMapper());

    OpenTcsTransportOrderEvent event = parser.parseTransportOrderEvent(payload);

    assertThat(event.missionNo()).isEqualTo("M202602100001");
    assertThat(event.taskNo()).isEqualTo("T202602090001");
    assertThat(event.transportOrderState()).isEqualTo("BEING_PROCESSED");
    assertThat(event.currentDriveOrderIndex()).isEqualTo(0);
    assertThat(event.currentPointId()).isEqualTo("Point-0001");
    assertThat(event.processingVehicle()).isEqualTo("AGV_01");
    assertThat(event.eventTime().toString()).isEqualTo("2026-02-10T10:35:21Z");
  }

  @Test
  void shouldResolveCurrentPointFromRouteStepInsteadOfDestination() {
    String payload = """
        {
          "eventTime": "2026-02-10T10:35:21Z",
          "currentObjectState": {
            "name": "M202602100002",
            "state": "BEING_PROCESSED",
            "currentDriveOrderIndex": 1,
            "currentRouteStepIndex": 0,
            "driveOrders": [
              {
                "destination": {
                  "destination": "Point-0001",
                  "operation": "MOVE",
                  "properties": {}
                },
                "state": "FINISHED"
              },
              {
                "destination": {
                  "destination": "Point-0004",
                  "operation": "MOVE",
                  "properties": {}
                },
                "route": {
                  "steps": [
                    {
                      "sourcePoint": "Point-0001",
                      "destinationPoint": "Point-0003"
                    },
                    {
                      "sourcePoint": "Point-0003",
                      "destinationPoint": "Point-0004"
                    }
                  ]
                },
                "state": "TRAVELLING"
              }
            ],
            "processingVehicle": "AGV_01",
            "properties": {
              "task_no": "T202602090002"
            }
          },
          "previousObjectState": null
        }
        """;
    OpenTcsSsePayloadParser parser = new OpenTcsSsePayloadParser(new ObjectMapper());

    OpenTcsTransportOrderEvent event = parser.parseTransportOrderEvent(payload);

    assertThat(event.missionNo()).isEqualTo("M202602100002");
    assertThat(event.currentPointId()).isEqualTo("Point-0001");
  }
}
