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
    String rcsStatus
) {

  public MissionCallbackTarget {
    missionNo = requireNonBlank(missionNo, "missionNo");
    taskNo = requireNonBlank(taskNo, "taskNo");
    callbackUrl = requireNonBlank(callbackUrl, "callbackUrl");
    traceId = normalizeTracingField(traceId);
    requestId = normalizeTracingField(requestId);
    rcsStatus = normalizeStatus(rcsStatus);
  }

  public MissionCallbackTarget(
      String missionNo,
      String taskNo,
      String callbackUrl,
      String traceId,
      String requestId
  ) {
    this(missionNo, taskNo, callbackUrl, traceId, requestId, "RECEIVED");
  }

  public MissionCallbackTarget withRcsStatus(String status) {
    return new MissionCallbackTarget(missionNo, taskNo, callbackUrl, traceId, requestId, status);
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
}
