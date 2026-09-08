// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.idem;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.nio.file.Path;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class FileIdempotencyStoreTest {

  @TempDir
  Path tempDir;

  @Test
  void shouldReloadPersistedIdempotencyRecord() {
    Path storageFile = tempDir.resolve("idempotency-store.json");
    IdempotencyRecord record = new IdempotencyRecord(
        "MISSION_CREATE",
        "M202604150001",
        "request-hash",
        "{\"missionNo\":\"M202604150001\"}",
        "SUCCESS"
    );
    FileIdempotencyStore store = new FileIdempotencyStore(storageFile, new ObjectMapper());
    store.save(record);

    FileIdempotencyStore reloadedStore = new FileIdempotencyStore(storageFile, new ObjectMapper());

    assertThat(
        reloadedStore.findByBizTypeAndBizKey("MISSION_CREATE", "M202604150001")
    ).contains(record);
  }
}
