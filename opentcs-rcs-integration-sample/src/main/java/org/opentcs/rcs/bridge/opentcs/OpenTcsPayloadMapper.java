// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Objects;
import org.opentcs.rcs.api.dto.Mission;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsDestination;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsProperty;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsTransportOrderReq;

/**
 * Maps mission model into openTCS web API payload.
 */
public class OpenTcsPayloadMapper {

  public OpenTcsTransportOrderReq toTransportOrderReq(Mission mission) {
    Objects.requireNonNull(mission, "mission");
    Instant deadline = deadlineFromPriority(mission.priority());
    return new OpenTcsTransportOrderReq(
        false,
        "MOVE",
        deadline.toString(),
        List.of(
            new OpenTcsDestination(mission.fromPoint(), "MOVE", List.of()),
            new OpenTcsDestination(mission.toPoint(), "MOVE", List.of())
        ),
        List.of(
            new OpenTcsProperty("mission_no", mission.missionNo()),
            new OpenTcsProperty("task_no", mission.taskNo()),
            new OpenTcsProperty("pallet_no", mission.palletNo()),
            new OpenTcsProperty("priority", String.valueOf(mission.priority())),
            new OpenTcsProperty("callback_url", mission.callbackUrl())
        )
    );
  }

  /**
   * Maps priority to an earlier/later deadline.
   * Higher numeric priority means earlier deadline.
   */
  public Instant deadlineFromPriority(int priority) {
    int normalized = Math.min(Math.max(priority, 1), 100);
    return Instant.now().plus(101 - normalized, ChronoUnit.MINUTES);
  }
}
