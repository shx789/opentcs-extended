// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.agvcommand;

import java.time.Instant;

/**
 * AGV command outbox status exposed through HTTP APIs.
 */
public record AgvCommandStatusResp(
    String missionNo,
    String taskNo,
    String commandStage,
    String idemKey,
    String payloadJson,
    String status,
    int retryCount,
    Instant nextRetryAt,
    String lastError
) {

  public static AgvCommandStatusResp from(AgvCommandOutboxEntry entry) {
    return new AgvCommandStatusResp(
        entry.missionNo(),
        entry.taskNo(),
        entry.commandStage(),
        entry.idemKey(),
        entry.payloadJson(),
        entry.status(),
        entry.retryCount(),
        entry.nextRetryAt(),
        entry.lastError()
    );
  }
}
