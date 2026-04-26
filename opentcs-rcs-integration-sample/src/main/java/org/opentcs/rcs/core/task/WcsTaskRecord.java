// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.task;

import java.util.Objects;

/**
 * Aggregated WCS task record keyed by bizTaskNo.
 */
public record WcsTaskRecord(
    String bizTaskNo,
    WcsTaskType taskType,
    String missionNo,
    String taskNo,
    String plcJobNo,
    WcsTaskStatus rcsStatus,
    String fromPoint,
    String toPoint,
    String palletNo,
    Integer priority,
    String callbackUrl,
    String traceId,
    String requestId,
    String createdAt,
    String updatedAt
) {

  public WcsTaskRecord {
    bizTaskNo = requireNonBlank(bizTaskNo, "bizTaskNo");
    taskType = Objects.requireNonNull(taskType, "taskType");
    missionNo = requireNonBlank(missionNo, "missionNo");
    taskNo = requireNonBlank(taskNo, "taskNo");
    plcJobNo = normalizeOptional(plcJobNo);
    rcsStatus = Objects.requireNonNull(rcsStatus, "rcsStatus");
    fromPoint = requireNonBlank(fromPoint, "fromPoint");
    toPoint = requireNonBlank(toPoint, "toPoint");
    palletNo = requireNonBlank(palletNo, "palletNo");
    callbackUrl = requireNonBlank(callbackUrl, "callbackUrl");
    traceId = normalizeTraceField(traceId);
    requestId = normalizeTraceField(requestId);
    createdAt = requireNonBlank(createdAt, "createdAt");
    updatedAt = requireNonBlank(updatedAt, "updatedAt");
  }

  public WcsTaskRecord withStatus(WcsTaskStatus status, String newUpdatedAt) {
    return new WcsTaskRecord(
        bizTaskNo,
        taskType,
        missionNo,
        taskNo,
        plcJobNo,
        status,
        fromPoint,
        toPoint,
        palletNo,
        priority,
        callbackUrl,
        traceId,
        requestId,
        createdAt,
        requireNonBlank(newUpdatedAt, "updatedAt")
    );
  }

  private static String requireNonBlank(String value, String fieldName) {
    Objects.requireNonNull(value, fieldName);
    if (value.isBlank()) {
      throw new IllegalArgumentException(fieldName + " must not be blank");
    }
    return value.trim();
  }

  private static String normalizeOptional(String value) {
    if (value == null || value.isBlank()) {
      return null;
    }
    return value.trim();
  }

  private static String normalizeTraceField(String value) {
    if (value == null || value.isBlank()) {
      return "legacy";
    }
    return value.trim();
  }
}
