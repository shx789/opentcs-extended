// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.task;

/**
 * Centralized transition guard for WCS task status state machine.
 */
public final class WcsTaskStatusTransitions {

  private WcsTaskStatusTransitions() {
  }

  public static boolean canTransit(WcsTaskStatus from, WcsTaskStatus to) {
    if (from == to) {
      return true;
    }
    return switch (from) {
      case RECEIVED -> to == WcsTaskStatus.IN_PROGRESS
          || to == WcsTaskStatus.WAIT_PLC
          || to == WcsTaskStatus.DONE
          || to == WcsTaskStatus.FAILED
          || to == WcsTaskStatus.CANCELED;
      case IN_PROGRESS -> to == WcsTaskStatus.WAIT_PLC
          || to == WcsTaskStatus.DONE
          || to == WcsTaskStatus.FAILED
          || to == WcsTaskStatus.CANCELED;
      case WAIT_PLC -> to == WcsTaskStatus.DONE
          || to == WcsTaskStatus.FAILED
          || to == WcsTaskStatus.CANCELED;
      case DONE, FAILED, CANCELED -> false;
    };
  }
}
