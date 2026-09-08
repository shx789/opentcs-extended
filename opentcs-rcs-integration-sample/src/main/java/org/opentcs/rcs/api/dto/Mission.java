// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.dto;

/**
 * Mission aggregate used by integration services.
 */
public record Mission(
    String missionNo,
    String taskNo,
    String fromPoint,
    String toPoint,
    String palletNo,
    int priority,
    String callbackUrl,
    String missionType,
    String fromOperation,
    String toOperation
) {
  public Mission(
      String missionNo,
      String taskNo,
      String fromPoint,
      String toPoint,
      String palletNo,
      int priority,
      String callbackUrl
  ) {
    this(missionNo, taskNo, fromPoint, toPoint, palletNo, priority, callbackUrl, null, null, null);
  }
}
