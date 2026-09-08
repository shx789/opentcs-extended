// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.idem;

/**
 * Stored idempotency metadata and response payload.
 */
public record IdempotencyRecord(
    String bizType,
    String bizKey,
    String requestHash,
    String responseJson,
    String status
) {
}
