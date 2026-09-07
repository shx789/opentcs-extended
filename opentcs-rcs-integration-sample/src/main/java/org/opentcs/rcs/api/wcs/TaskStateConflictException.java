// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

/**
 * Exception for invalid task status transitions.
 */
public class TaskStateConflictException
    extends
      IllegalStateException {

  public TaskStateConflictException(String message) {
    super(message);
  }
}
