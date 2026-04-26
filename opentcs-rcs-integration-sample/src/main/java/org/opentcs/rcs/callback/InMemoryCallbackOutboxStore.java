// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

import java.time.Instant;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

/**
 * In-memory callback outbox storage for local runs/tests.
 */
public class InMemoryCallbackOutboxStore
    implements CallbackOutboxStore {

  private final ConcurrentMap<String, CallbackOutboxEntry> entries = new ConcurrentHashMap<>();

  @Override
  public void save(CallbackOutboxEntry entry) {
    entries.put(entry.idemKey(), copy(entry));
  }

  @Override
  public List<CallbackOutboxEntry> findDue(Instant now, int limit) {
    return entries.values().stream()
        .filter(
            e -> ("PENDING".equals(e.status()) || "FAILED".equals(e.status()))
                && (e.nextRetryAt() == null || !e.nextRetryAt().isAfter(now))
        )
        .sorted(Comparator.comparing(CallbackOutboxEntry::idemKey))
        .limit(limit)
        .map(this::copy)
        .toList();
  }

  private CallbackOutboxEntry copy(CallbackOutboxEntry source) {
    return new CallbackOutboxEntry(
        source.missionNo(),
        source.callbackUrl(),
        source.payloadJson(),
        source.idemKey(),
        source.status(),
        source.retryCount(),
        source.nextRetryAt(),
        source.lastError()
    );
  }
}
