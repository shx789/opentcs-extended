// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

import java.time.Instant;
import java.util.List;

/**
 * Storage for callback outbox entries.
 */
public interface CallbackOutboxStore {

  /**
   * Saves (or updates) an outbox entry by its idempotency key.
   */
  void save(CallbackOutboxEntry entry);

  List<CallbackOutboxEntry> findDue(Instant now, int limit);
}
