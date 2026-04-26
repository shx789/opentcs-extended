// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

import java.time.Instant;
import java.util.List;
import java.util.Objects;

/**
 * Processes due callback outbox entries and applies retry policy.
 */
public class CallbackRetryProcessor {

  private final CallbackOutboxStore store;
  private final CallbackSender sender;

  public CallbackRetryProcessor(CallbackOutboxStore store, CallbackSender sender) {
    this.store = Objects.requireNonNull(store, "store");
    this.sender = Objects.requireNonNull(sender, "sender");
  }

  public void processDue(int batchSize) {
    List<CallbackOutboxEntry> dueEntries = store.findDue(Instant.now(), batchSize);
    for (CallbackOutboxEntry entry : dueEntries) {
      try {
        sender.send(entry.callbackUrl(), entry.payloadJson());
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
      case 1 -> 60L;
      case 2 -> 120L;
      case 3 -> 300L;
      case 4 -> 600L;
      default -> 1800L;
    };
  }
}
