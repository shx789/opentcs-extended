// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.mission;

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
 * File-backed mission callback target store for local persistence.
 */
public class FileMissionStore
    implements MissionStore {

  private final Path storageFile;
  private final ObjectMapper objectMapper;
  private final Map<String, MissionCallbackTarget> targetsByMissionNo = new LinkedHashMap<>();

  public FileMissionStore(Path storageFile, ObjectMapper objectMapper) {
    this.storageFile = Objects.requireNonNull(storageFile, "storageFile").toAbsolutePath();
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
    loadFromDisk();
  }

  @Override
  public synchronized void save(MissionCallbackTarget target) {
    Objects.requireNonNull(target, "target");
    targetsByMissionNo.put(target.missionNo(), target);
    persistToDisk();
  }

  @Override
  public synchronized Optional<MissionCallbackTarget> findByMissionNo(String missionNo) {
    return Optional.ofNullable(targetsByMissionNo.get(missionNo));
  }

  private void loadFromDisk() {
    if (!Files.exists(storageFile)) {
      return;
    }
    try {
      if (Files.size(storageFile) == 0L) {
        return;
      }
      List<MissionCallbackTarget> loadedTargets = objectMapper.readValue(
          storageFile.toFile(),
          new TypeReference<List<MissionCallbackTarget>>() {
          }
      );
      for (MissionCallbackTarget target : loadedTargets) {
        targetsByMissionNo.put(target.missionNo(), target);
      }
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not read mission store: " + storageFile, exc);
    }
  }

  private void persistToDisk() {
    Path parentDir = storageFile.getParent();
    if (parentDir != null) {
      try {
        Files.createDirectories(parentDir);
      }
      catch (IOException exc) {
        throw new IllegalStateException("Could not create mission store directory: " + parentDir, exc);
      }
    }
    Path tempFile = storageFile.resolveSibling(storageFile.getFileName() + ".tmp");
    try {
      objectMapper.writeValue(tempFile.toFile(), targetsByMissionNo.values());
      moveTempFile(tempFile, storageFile);
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not persist mission store: " + storageFile, exc);
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
