// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.agvcommand;

import java.time.Instant;
import java.util.List;
import java.util.Objects;

/**
 * Sends due AGV command outbox entries and applies retry policy.
 */
public class AgvCommandRetryProcessor {

  private final AgvCommandOutboxStore store;
  private final AgvCommandSender sender;

  public AgvCommandRetryProcessor(AgvCommandOutboxStore store, AgvCommandSender sender) {
    this.store = Objects.requireNonNull(store, "store");
    this.sender = Objects.requireNonNull(sender, "sender");
  }

  public void processDue(int batchSize) {
    List<AgvCommandOutboxEntry> dueEntries = store.findDue(Instant.now(), batchSize);
    for (AgvCommandOutboxEntry entry : dueEntries) {
      try {
        sender.send(entry.payloadJson());
        entry.setStatus("SUCCESS");
        entry.setLastError(null);
        entry.setNextRetryAt(null);
        store.save(entry);
      }
      catch (Exception exc) {
        int retryCount = entry.retryCount() + 1;
        entry.setRetryCount(retryCount);
        entry.setStatus("FAILED");
        entry.setLastError(exc.getMessage());
        entry.setNextRetryAt(Instant.now().plusSeconds(backoffSeconds(retryCount)));
        store.save(entry);
      }
    }
  }

  long backoffSeconds(int retryCount) {
    return switch (retryCount) {
      case 1 -> 5L;
      case 2 -> 15L;
      case 3 -> 30L;
      case 4 -> 60L;
      default -> 300L;
    };
  }
}
