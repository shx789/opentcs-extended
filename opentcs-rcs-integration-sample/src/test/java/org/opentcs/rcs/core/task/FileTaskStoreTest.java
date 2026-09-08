// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.task;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.nio.file.Path;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class FileTaskStoreTest {

  @TempDir
  Path tempDir;

  @Test
  void shouldReloadPersistedTaskRecord() {
    Path storageFile = tempDir.resolve("task-store.json");
    FileTaskStore store = new FileTaskStore(storageFile, new ObjectMapper());
    WcsTaskRecord record = new WcsTaskRecord(
        "BIZ-IN-001",
        WcsTaskType.INBOUND,
        "M_BIZ-IN-001",
        "T_BIZ-IN-001",
        null,
        WcsTaskStatus.RECEIVED,
        "Point-0020",
        "Point-0026",
        "PLT0001",
        80,
        "http://127.0.0.1:8080/demo/wcs/callback",
        "trace-1",
        "request-1",
        "2026-04-21T11:00:00Z",
        "2026-04-21T11:00:00Z"
    );
    store.save(record);

    FileTaskStore reloaded = new FileTaskStore(storageFile, new ObjectMapper());

    assertThat(reloaded.findByBizTaskNo("BIZ-IN-001")).contains(record);
    assertThat(reloaded.findByMissionNo("M_BIZ-IN-001")).contains(record);
  }
}
