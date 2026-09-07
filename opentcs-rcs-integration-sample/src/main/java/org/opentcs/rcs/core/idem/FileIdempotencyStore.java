// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.idem;

import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;

/**
 * File-backed idempotency store for local persistence.
 */
public class FileIdempotencyStore
    implements
      IdempotencyStore {

  private final Path storageFile;
  private final ObjectMapper objectMapper;
  private final Map<String, IdempotencyRecord> recordsByKey = new LinkedHashMap<>();

  public FileIdempotencyStore(Path storageFile, ObjectMapper objectMapper) {
    this.storageFile = Objects.requireNonNull(storageFile, "storageFile").toAbsolutePath();
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
    loadFromDisk();
  }

  @Override
  public synchronized Optional<IdempotencyRecord> findByBizTypeAndBizKey(
      String bizType, String bizKey
  ) {
    return Optional.ofNullable(recordsByKey.get(makeKey(bizType, bizKey)));
  }

  @Override
  public synchronized void save(IdempotencyRecord record) {
    Objects.requireNonNull(record, "record");
    recordsByKey.put(makeKey(record.bizType(), record.bizKey()), record);
    persistToDisk();
  }

  private void loadFromDisk() {
    if (!Files.exists(storageFile)) {
      return;
    }
    try {
      if (Files.size(storageFile) == 0L) {
        return;
      }
      List<IdempotencyRecord> loadedRecords = objectMapper.readValue(
          storageFile.toFile(),
          new TypeReference<List<IdempotencyRecord>>() {
          }
      );
      for (IdempotencyRecord record : loadedRecords) {
        recordsByKey.put(makeKey(record.bizType(), record.bizKey()), record);
      }
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not read idempotency store: " + storageFile, exc);
    }
  }

  private void persistToDisk() {
    Path parentDir = storageFile.getParent();
    if (parentDir != null) {
      try {
        Files.createDirectories(parentDir);
      }
      catch (IOException exc) {
        throw new IllegalStateException(
            "Could not create idempotency store directory: " + parentDir,
            exc
        );
      }
    }
    Path tempFile = storageFile.resolveSibling(storageFile.getFileName() + ".tmp");
    try {
      objectMapper.writeValue(tempFile.toFile(), recordsByKey.values());
      moveTempFile(tempFile, storageFile);
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not persist idempotency store: " + storageFile, exc);
    }
  }

  private void moveTempFile(Path source, Path target)
      throws IOException {
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

  private String makeKey(String bizType, String bizKey) {
    return bizType + "::" + bizKey;
  }
}
