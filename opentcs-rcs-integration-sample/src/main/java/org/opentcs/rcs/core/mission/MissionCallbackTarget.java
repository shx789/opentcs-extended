// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.mission;

import java.util.Objects;

/**
 * Mission callback lookup result.
 */
public record MissionCallbackTarget(
    String missionNo,
    String taskNo,
    String callbackUrl,
    String traceId,
    String requestId,
    String rcsStatus,
    String fromPoint,
    String toPoint,
    String palletNo,
    Integer priority
) {

  public MissionCallbackTarget {
    missionNo = requireNonBlank(missionNo, "missionNo");
    taskNo = requireNonBlank(taskNo, "taskNo");
    callbackUrl = requireNonBlank(callbackUrl, "callbackUrl");
    traceId = normalizeTracingField(traceId);
    requestId = normalizeTracingField(requestId);
    rcsStatus = normalizeStatus(rcsStatus);
    fromPoint = normalizeOptional(fromPoint);
    toPoint = normalizeOptional(toPoint);
    palletNo = normalizeOptional(palletNo);
  }

  public MissionCallbackTarget(
      String missionNo,
      String taskNo,
      String callbackUrl,
      String traceId,
      String requestId
  ) {
    this(missionNo, taskNo, callbackUrl, traceId, requestId, "RECEIVED", null, null, null, null);
  }

  public MissionCallbackTarget(
      String missionNo,
      String taskNo,
      String callbackUrl,
      String traceId,
      String requestId,
      String fromPoint,
      String toPoint,
      String palletNo,
      Integer priority
  ) {
    this(
        missionNo,
        taskNo,
        callbackUrl,
        traceId,
        requestId,
        "RECEIVED",
        fromPoint,
        toPoint,
        palletNo,
        priority
    );
  }

  public MissionCallbackTarget withRcsStatus(String status) {
    return new MissionCallbackTarget(
        missionNo,
        taskNo,
        callbackUrl,
        traceId,
        requestId,
        status,
        fromPoint,
        toPoint,
        palletNo,
        priority
    );
  }

  private static String normalizeStatus(String value) {
    if (value == null || value.isBlank()) {
      return "RECEIVED";
    }
    return value.trim();
  }

  private static String requireNonBlank(String value, String fieldName) {
    Objects.requireNonNull(value, fieldName);
    if (value.isBlank()) {
      throw new IllegalArgumentException(fieldName + " must not be blank");
    }
    return value;
  }

  private static String normalizeTracingField(String value) {
    if (value == null || value.isBlank()) {
      return "legacy";
    }
    return value.trim();
  }

  private static String normalizeOptional(String value) {
    if (value == null || value.isBlank()) {
      return null;
    }
    return value.trim();
  }
}
