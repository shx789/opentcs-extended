// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule;
import java.nio.file.Path;
import java.time.Instant;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class FileCallbackOutboxStoreTest {

  @TempDir
  Path tempDir;

  @Test
  void shouldReloadAndUpdatePersistedOutboxEntry() {
    Path storageFile = tempDir.resolve("callback-outbox-store.json");
    ObjectMapper objectMapper = new ObjectMapper().registerModule(new JavaTimeModule());
    CallbackOutboxEntry entry = new CallbackOutboxEntry(
        "M202604150001",
        "http://127.0.0.1:8081/api/v1/wcs/agv/events",
        "{\"eventType\":\"DROPPED\"}",
        "M202604150001-DROPPED",
        "PENDING",
        0,
        Instant.parse("2026-04-15T10:35:21Z"),
        null
    );
    FileCallbackOutboxStore store = new FileCallbackOutboxStore(storageFile, objectMapper);
    store.save(entry);

    FileCallbackOutboxStore reloadedStore = new FileCallbackOutboxStore(storageFile, objectMapper);
    CallbackOutboxEntry loaded = reloadedStore.findDue(
        Instant.parse("2026-04-15T11:35:21Z"),
        10
    ).stream().findFirst().orElseThrow();
    assertThat(loaded.status()).isEqualTo("PENDING");

    loaded.setStatus("SUCCESS");
    loaded.setNextRetryAt(null);
    reloadedStore.save(loaded);

    FileCallbackOutboxStore afterUpdate = new FileCallbackOutboxStore(storageFile, objectMapper);
    assertThat(afterUpdate.findDue(Instant.parse("2026-04-15T11:35:21Z"), 10)).isEmpty();
  }
}
