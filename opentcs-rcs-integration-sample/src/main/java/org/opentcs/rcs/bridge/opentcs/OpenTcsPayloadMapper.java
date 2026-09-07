// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import org.opentcs.rcs.api.dto.Mission;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsDestination;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsProperty;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsTransportOrderReq;

/**
 * Maps mission model into openTCS web API payload.
 */
public class OpenTcsPayloadMapper {

  private static final String MISSION_TYPE_MOVE_ONLY = "MOVE_ONLY";
  private static final String MISSION_TYPE_NAVIGATION = "NAVIGATION";
  private static final String MISSION_TYPE_NAVIGATE = "NAVIGATE";
  private static final String OPERATION_MOVE = "MOVE";
  private static final String OPERATION_PICK = "PICK";
  private static final String OPERATION_DROP = "DROP";

  public OpenTcsTransportOrderReq toTransportOrderReq(Mission mission) {
    Objects.requireNonNull(mission, "mission");
    Instant deadline = deadlineFromPriority(mission.priority());
    return new OpenTcsTransportOrderReq(
        false,
        "MOVE",
        deadline.toString(),
        List.of(
            new OpenTcsDestination(
                destinationNameFor(mission.fromPoint(), operationFor(mission, true)),
                operationFor(mission, true),
                List.of()
            ),
            new OpenTcsDestination(
                destinationNameFor(mission.toPoint(), operationFor(mission, false)),
                operationFor(mission, false),
                List.of()
            )
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

  private String operationFor(Mission mission, boolean fromDestination) {
    String explicitOperation = fromDestination ? mission.fromOperation() : mission.toOperation();
    if (explicitOperation != null && !explicitOperation.isBlank()) {
      return normalizeOperation(explicitOperation);
    }

    if (isMoveOnly(mission.missionType())) {
      return OPERATION_MOVE;
    }
    return fromDestination ? OPERATION_PICK : OPERATION_DROP;
  }

  private String destinationNameFor(String pointName, String operation) {
    if ((operation.equals(OPERATION_PICK) || operation.equals(OPERATION_DROP))
        && !pointName.startsWith("LOC_")) {
      return "LOC_" + pointName;
    }
    return pointName;
  }
  private boolean isMoveOnly(String missionType) {
    String normalized = normalizeOperation(missionType);
    return normalized.equals(MISSION_TYPE_MOVE_ONLY)
        || normalized.equals(MISSION_TYPE_NAVIGATION)
        || normalized.equals(MISSION_TYPE_NAVIGATE)
        || normalized.equals(OPERATION_MOVE);
  }

  private String normalizeOperation(String value) {
    if (value == null) {
      return "";
    }
    return value.trim().toUpperCase(Locale.ROOT).replace('-', '_');
  }
}
