// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

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

/**
 * File-backed callback outbox store for local persistence.
 */
public class FileCallbackOutboxStore
    implements CallbackOutboxStore {

  private final Path storageFile;
  private final ObjectMapper objectMapper;
  private final Map<String, CallbackOutboxEntry> entriesByIdemKey = new LinkedHashMap<>();

  public FileCallbackOutboxStore(Path storageFile, ObjectMapper objectMapper) {
    this.storageFile = Objects.requireNonNull(storageFile, "storageFile").toAbsolutePath();
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
    loadFromDisk();
  }

  @Override
  public synchronized void save(CallbackOutboxEntry entry) {
    Objects.requireNonNull(entry, "entry");
    entriesByIdemKey.put(entry.idemKey(), copy(entry));
    persistToDisk();
  }

  @Override
  public synchronized List<CallbackOutboxEntry> findDue(Instant now, int limit) {
    return entriesByIdemKey.values().stream()
        .filter(
            e -> ("PENDING".equals(e.status()) || "FAILED".equals(e.status()))
                && (e.nextRetryAt() == null || !e.nextRetryAt().isAfter(now))
        )
        .sorted(Comparator.comparing(CallbackOutboxEntry::idemKey))
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
      List<StoredOutboxEntry> loadedEntries = objectMapper.readValue(
          storageFile.toFile(),
          new TypeReference<List<StoredOutboxEntry>>() {
          }
      );
      for (StoredOutboxEntry entry : loadedEntries) {
        entriesByIdemKey.put(entry.idemKey(), entry.toCallbackOutboxEntry());
      }
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not read callback outbox store: " + storageFile, exc);
    }
  }

  private void persistToDisk() {
    Path parentDir = storageFile.getParent();
    if (parentDir != null) {
      try {
        Files.createDirectories(parentDir);
      }
      catch (IOException exc) {
        throw new IllegalStateException("Could not create callback store directory: " + parentDir, exc);
      }
    }
    Path tempFile = storageFile.resolveSibling(storageFile.getFileName() + ".tmp");
    List<StoredOutboxEntry> entries = entriesByIdemKey.values().stream()
        .sorted(Comparator.comparing(CallbackOutboxEntry::idemKey))
        .map(StoredOutboxEntry::from)
        .toList();
    try {
      objectMapper.writeValue(tempFile.toFile(), entries);
      moveTempFile(tempFile, storageFile);
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not persist callback outbox store: " + storageFile, exc);
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

  private record StoredOutboxEntry(
      String missionNo,
      String callbackUrl,
      String payloadJson,
      String idemKey,
      String status,
      int retryCount,
      Instant nextRetryAt,
      String lastError
  ) {

    private static StoredOutboxEntry from(CallbackOutboxEntry entry) {
      return new StoredOutboxEntry(
          entry.missionNo(),
          entry.callbackUrl(),
          entry.payloadJson(),
          entry.idemKey(),
          entry.status(),
          entry.retryCount(),
          entry.nextRetryAt(),
          entry.lastError()
      );
    }

    private CallbackOutboxEntry toCallbackOutboxEntry() {
      return new CallbackOutboxEntry(
          missionNo,
          callbackUrl,
          payloadJson,
          idemKey,
          status,
          retryCount,
          nextRetryAt,
          lastError
      );
    }
  }
}
