// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.task;

/**
 * WCS-side task status.
 */
public enum WcsTaskStatus {
  RECEIVED,
  IN_PROGRESS,
  WAIT_PLC,
  DONE,
  FAILED,
  CANCELED
}
