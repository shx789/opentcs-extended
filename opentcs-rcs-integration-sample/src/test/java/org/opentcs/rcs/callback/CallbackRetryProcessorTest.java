// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.concurrent.atomic.AtomicInteger;
import org.junit.jupiter.api.Test;

class CallbackRetryProcessorTest {

  @Test
  void shouldMarkSuccessWhenCallbackSendSucceeds() {
    InMemoryCallbackOutboxStore store = new InMemoryCallbackOutboxStore();
    CallbackOutboxService service = new CallbackOutboxService(store, new ObjectMapper());
    service.enqueue("M1", "http://wcs/callback", new Payload("ARRIVED_FROM"), "M1-ARRIVED_FROM");

    CallbackRetryProcessor processor = new CallbackRetryProcessor(
        store,
        (url, payload) -> {
          // no-op: simulate success
        }
    );
    processor.processDue(10);

    CallbackOutboxEntry entry = store.findDue(Instant.now().plusSeconds(1), 10).stream()
        .findFirst()
        .orElse(null);
    assertThat(entry).isNull();
  }

  @Test
  void shouldIncreaseRetryCountWhenCallbackSendFails() {
    InMemoryCallbackOutboxStore store = new InMemoryCallbackOutboxStore();
    CallbackOutboxService service = new CallbackOutboxService(store, new ObjectMapper());
    service.enqueue("M2", "http://wcs/callback", new Payload("FAILED"), "M2-FAILED");

    AtomicInteger attempts = new AtomicInteger();
    CallbackRetryProcessor processor = new CallbackRetryProcessor(
        store,
        (url, payload) -> {
          attempts.incrementAndGet();
          throw new IllegalStateException("wcs unavailable");
        }
    );
    processor.processDue(10);

    CallbackOutboxEntry entry = store.findDue(Instant.now().plus(1, ChronoUnit.HOURS), 10).stream()
        .findFirst()
        .orElseThrow();
    assertThat(attempts.get()).isEqualTo(1);
    assertThat(entry.status()).isEqualTo("FAILED");
    assertThat(entry.retryCount()).isEqualTo(1);
    assertThat(entry.nextRetryAt()).isAfter(Instant.now().plusSeconds(50));
  }

  private record Payload(String eventType) {
  }
}
