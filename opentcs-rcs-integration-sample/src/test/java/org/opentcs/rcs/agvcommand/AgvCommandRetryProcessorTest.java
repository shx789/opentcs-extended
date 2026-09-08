// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.agvcommand;

import static org.assertj.core.api.Assertions.assertThat;

import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import org.junit.jupiter.api.Test;

class AgvCommandRetryProcessorTest {

  @Test
  void shouldMarkCommandSuccessWhenSenderSucceeds() {
    InMemoryAgvCommandOutboxStore store = new InMemoryAgvCommandOutboxStore();
    store.save(entry("PENDING", 0, Instant.now(), null));
    List<String> sentPayloads = new ArrayList<>();
    AgvCommandRetryProcessor processor = new AgvCommandRetryProcessor(store, sentPayloads::add);

    processor.processDue(10);

    AgvCommandOutboxEntry stored = store.findByIdemKey("M1:MISSION_START").orElseThrow();
    assertThat(sentPayloads).containsExactly("{\"cmd\":\"start\"}");
    assertThat(stored.status()).isEqualTo("SUCCESS");
    assertThat(stored.retryCount()).isZero();
    assertThat(stored.nextRetryAt()).isNull();
    assertThat(stored.lastError()).isNull();
  }

  @Test
  void shouldMarkCommandFailedAndBackoffWhenSenderFails() {
    InMemoryAgvCommandOutboxStore store = new InMemoryAgvCommandOutboxStore();
    store.save(entry("PENDING", 0, Instant.now(), null));
    AgvCommandRetryProcessor processor = new AgvCommandRetryProcessor(
        store,
        payload -> {
          throw new IllegalStateException("broker unavailable");
        }
    );

    processor.processDue(10);

    AgvCommandOutboxEntry stored = store.findByIdemKey("M1:MISSION_START").orElseThrow();
    assertThat(stored.status()).isEqualTo("FAILED");
    assertThat(stored.retryCount()).isEqualTo(1);
    assertThat(stored.nextRetryAt()).isAfter(Instant.now());
    assertThat(stored.lastError()).isEqualTo("broker unavailable");
  }

  @Test
  void shouldSkipCommandsThatAreNotDueYet() {
    InMemoryAgvCommandOutboxStore store = new InMemoryAgvCommandOutboxStore();
    store.save(entry("FAILED", 1, Instant.now().plusSeconds(60), "broker unavailable"));
    List<String> sentPayloads = new ArrayList<>();
    AgvCommandRetryProcessor processor = new AgvCommandRetryProcessor(store, sentPayloads::add);

    processor.processDue(10);

    AgvCommandOutboxEntry stored = store.findByIdemKey("M1:MISSION_START").orElseThrow();
    assertThat(sentPayloads).isEmpty();
    assertThat(stored.status()).isEqualTo("FAILED");
    assertThat(stored.retryCount()).isEqualTo(1);
  }

  private AgvCommandOutboxEntry entry(
      String status,
      int retryCount,
      Instant nextRetryAt,
      String lastError
  ) {
    return new AgvCommandOutboxEntry(
        "M1",
        "T1",
        "MISSION_START",
        "M1:MISSION_START",
        "{\"cmd\":\"start\"}",
        status,
        retryCount,
        nextRetryAt,
        lastError
    );
  }
}
