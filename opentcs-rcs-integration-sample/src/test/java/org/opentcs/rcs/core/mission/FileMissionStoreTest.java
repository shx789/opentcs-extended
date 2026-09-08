// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.mission;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.nio.file.Path;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class FileMissionStoreTest {

  @TempDir
  Path tempDir;

  @Test
  void shouldReloadPersistedMissionCallbackTarget() {
    Path storageFile = tempDir.resolve("mission-store.json");
    MissionCallbackTarget target = new MissionCallbackTarget(
        "M202604150001",
        "T202604150001",
        "/api/v1/wcs/agv/events",
        "trace-1",
        "request-1"
    );
    FileMissionStore store = new FileMissionStore(storageFile, new ObjectMapper());
    store.save(target);

    FileMissionStore reloadedStore = new FileMissionStore(storageFile, new ObjectMapper());

    assertThat(reloadedStore.findByMissionNo("M202604150001")).contains(target);
  }
}
