// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.idem;

/**
 * Exception for idempotency conflict on same business key with different payload.
 */
public class IdempotencyConflictException
    extends IllegalArgumentException {

  public IdempotencyConflictException(String message) {
    super(message);
  }
}
