// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.agvcommand;

import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.time.Instant;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;

/**
 * File-backed AGV command outbox store for local persistence.
 */
public class FileAgvCommandOutboxStore
    implements AgvCommandOutboxStore {

  private final Path storageFile;
  private final ObjectMapper objectMapper;
  private final Map<String, AgvCommandOutboxEntry> entriesByIdemKey = new LinkedHashMap<>();

  public FileAgvCommandOutboxStore(Path storageFile, ObjectMapper objectMapper) {
    this.storageFile = Objects.requireNonNull(storageFile, "storageFile").toAbsolutePath();
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
    loadFromDisk();
  }

  @Override
  public synchronized void save(AgvCommandOutboxEntry entry) {
    Objects.requireNonNull(entry, "entry");
    entriesByIdemKey.put(entry.idemKey(), copy(entry));
    persistToDisk();
  }

  @Override
  public synchronized Optional<AgvCommandOutboxEntry> findByIdemKey(String idemKey) {
    return Optional.ofNullable(entriesByIdemKey.get(idemKey)).map(this::copy);
  }

  @Override
  public synchronized List<AgvCommandOutboxEntry> findByMissionNo(String missionNo) {
    return entriesByIdemKey.values().stream()
        .filter(entry -> entry.missionNo().equals(missionNo))
        .sorted(Comparator.comparing(AgvCommandOutboxEntry::idemKey))
        .map(this::copy)
        .toList();
  }

  @Override
  public synchronized List<AgvCommandOutboxEntry> findDue(Instant now, int limit) {
    return entriesByIdemKey.values().stream()
        .filter(
            entry -> ("PENDING".equals(entry.status()) || "FAILED".equals(entry.status()))
                && (entry.nextRetryAt() == null || !entry.nextRetryAt().isAfter(now))
        )
        .sorted(Comparator.comparing(AgvCommandOutboxEntry::idemKey))
        .limit(limit)
        .map(this::copy)
        .toList();
  }

  private void loadFromDisk() {
    if (!Files.exists(storageFile)) {
      return;
    }
    try {
      if (Files.size(storageFile) == 0L) {
        return;
      }
      List<StoredAgvCommandEntry> loadedEntries = objectMapper.readValue(
          storageFile.toFile(),
          new TypeReference<List<StoredAgvCommandEntry>>() {
          }
      );
      for (StoredAgvCommandEntry entry : loadedEntries) {
        entriesByIdemKey.put(entry.idemKey(), entry.toAgvCommandOutboxEntry());
      }
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not read AGV command outbox store: " + storageFile, exc);
    }
  }

  private void persistToDisk() {
    Path parentDir = storageFile.getParent();
    if (parentDir != null) {
      try {
        Files.createDirectories(parentDir);
      }
      catch (IOException exc) {
        throw new IllegalStateException("Could not create AGV command store directory: " + parentDir, exc);
      }
    }
    Path tempFile = storageFile.resolveSibling(storageFile.getFileName() + ".tmp");
    List<StoredAgvCommandEntry> entries = entriesByIdemKey.values().stream()
        .sorted(Comparator.comparing(AgvCommandOutboxEntry::idemKey))
        .map(StoredAgvCommandEntry::from)
        .toList();
    try {
      objectMapper.writeValue(tempFile.toFile(), entries);
      moveTempFile(tempFile, storageFile);
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not persist AGV command outbox store: " + storageFile, exc);
    }
  }

  private void moveTempFile(Path source, Path target) throws IOException {
    try {
      Files.move(
          source,
          target,
          StandardCopyOption.REPLACE_EXISTING,
          StandardCopyOption.ATOMIC_MOVE
      );
    }
    catch (AtomicMoveNotSupportedException exc) {
      Files.move(source, target, StandardCopyOption.REPLACE_EXISTING);
    }
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

  private record StoredAgvCommandEntry(
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

    private static StoredAgvCommandEntry from(AgvCommandOutboxEntry entry) {
      return new StoredAgvCommandEntry(
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

    private AgvCommandOutboxEntry toAgvCommandOutboxEntry() {
      return new AgvCommandOutboxEntry(
          missionNo,
          taskNo,
          commandStage,
          idemKey,
          payloadJson,
          status,
          retryCount,
          nextRetryAt,
          lastError
      );
    }
  }
}
