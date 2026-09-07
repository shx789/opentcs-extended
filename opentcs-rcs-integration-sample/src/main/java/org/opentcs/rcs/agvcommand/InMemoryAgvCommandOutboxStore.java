// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.agvcommand;

import java.time.Instant;
import java.util.Comparator;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

/**
 * In-memory AGV command outbox storage for local runs/tests.
 */
public class InMemoryAgvCommandOutboxStore
    implements
      AgvCommandOutboxStore {

  private final ConcurrentMap<String, AgvCommandOutboxEntry> entries = new ConcurrentHashMap<>();

  @Override
  public void save(AgvCommandOutboxEntry entry) {
    entries.put(entry.idemKey(), copy(entry));
  }

  @Override
  public Optional<AgvCommandOutboxEntry> findByIdemKey(String idemKey) {
    return Optional.ofNullable(entries.get(idemKey)).map(this::copy);
  }

  @Override
  public List<AgvCommandOutboxEntry> findByMissionNo(String missionNo) {
    return entries.values().stream()
        .filter(entry -> entry.missionNo().equals(missionNo))
        .sorted(Comparator.comparing(AgvCommandOutboxEntry::idemKey))
        .map(this::copy)
        .toList();
  }

  @Override
  public List<AgvCommandOutboxEntry> findDue(Instant now, int limit) {
    return entries.values().stream()
        .filter(
            entry -> ("PENDING".equals(entry.status()) || "FAILED".equals(entry.status()))
                && (entry.nextRetryAt() == null || !entry.nextRetryAt().isAfter(now))
        )
        .sorted(Comparator.comparing(AgvCommandOutboxEntry::idemKey))
        .limit(limit)
        .map(this::copy)
        .toList();
  }

  private AgvCommandOutboxEntry copy(AgvCommandOutboxEntry source) {
    return new AgvCommandOutboxEntry(
        source.missionNo(),
        source.taskNo(),
        source.commandStage(),
        source.idemKey(),
        source.payloadJson(),
        source.status(),
        source.retryCount(),
        source.nextRetryAt(),
        source.lastError()
    );
  }
}
