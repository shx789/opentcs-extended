// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.task;

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
 * File-backed task store for local persistence.
 */
public class FileTaskStore
    implements TaskStore {

  private final Path storageFile;
  private final ObjectMapper objectMapper;
  private final Map<String, WcsTaskRecord> recordsByBizTaskNo = new LinkedHashMap<>();
  private final Map<String, String> bizTaskNoByMissionNo = new LinkedHashMap<>();

  public FileTaskStore(Path storageFile, ObjectMapper objectMapper) {
    this.storageFile = Objects.requireNonNull(storageFile, "storageFile").toAbsolutePath();
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
    loadFromDisk();
  }

  @Override
  public synchronized void save(WcsTaskRecord record) {
    Objects.requireNonNull(record, "record");
    WcsTaskRecord oldRecord = recordsByBizTaskNo.put(record.bizTaskNo(), record);
    if (oldRecord != null) {
      bizTaskNoByMissionNo.remove(oldRecord.missionNo());
    }
    bizTaskNoByMissionNo.put(record.missionNo(), record.bizTaskNo());
    persistToDisk();
  }

  @Override
  public synchronized Optional<WcsTaskRecord> findByBizTaskNo(String bizTaskNo) {
    return Optional.ofNullable(recordsByBizTaskNo.get(bizTaskNo));
  }

  @Override
  public synchronized Optional<WcsTaskRecord> findByMissionNo(String missionNo) {
    return Optional.ofNullable(bizTaskNoByMissionNo.get(missionNo))
        .map(recordsByBizTaskNo::get);
  }

  private void loadFromDisk() {
    if (!Files.exists(storageFile)) {
      return;
    }
    try {
      if (Files.size(storageFile) == 0L) {
        return;
      }
      List<WcsTaskRecord> loadedRecords = objectMapper.readValue(
          storageFile.toFile(),
          new TypeReference<List<WcsTaskRecord>>() {
          }
      );
      for (WcsTaskRecord record : loadedRecords) {
        recordsByBizTaskNo.put(record.bizTaskNo(), record);
        bizTaskNoByMissionNo.put(record.missionNo(), record.bizTaskNo());
      }
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not read task store: " + storageFile, exc);
    }
  }

  private void persistToDisk() {
    Path parentDir = storageFile.getParent();
    if (parentDir != null) {
      try {
        Files.createDirectories(parentDir);
      }
      catch (IOException exc) {
        throw new IllegalStateException("Could not create task store directory: " + parentDir, exc);
      }
    }
    Path tempFile = storageFile.resolveSibling(storageFile.getFileName() + ".tmp");
    try {
      objectMapper.writeValue(tempFile.toFile(), recordsByBizTaskNo.values());
      moveTempFile(tempFile, storageFile);
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not persist task store: " + storageFile, exc);
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
}
