// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.dto;

/**
 * WCS callback event types.
 */
public enum WcsEventType {
  ARRIVED_FROM,
  PICKED,
  ARRIVED_TO,
  DROPPED,
  FAILED
}
