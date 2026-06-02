// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.time.Instant;
import java.util.Objects;

/**
 * Enqueues callback requests into outbox.
 */
public class CallbackOutboxService {

  private final CallbackOutboxStore store;
  private final ObjectMapper objectMapper;

  public CallbackOutboxService(CallbackOutboxStore store, ObjectMapper objectMapper) {
    this.store = Objects.requireNonNull(store, "store");
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
  }

  public void enqueue(String missionNo, String callbackUrl, Object payload, String idemKey) {
    if (store.findByIdemKey(idemKey).isPresent()) {
      return;
    }
    store.save(
        new CallbackOutboxEntry(
            missionNo,
            callbackUrl,
            toJson(payload),
            idemKey,
            "PENDING",
            0,
            Instant.now(),
            null
        )
    );
  }

  private String toJson(Object payload) {
    try {
      return objectMapper.writeValueAsString(payload);
    }
    catch (JsonProcessingException exc) {
      throw new IllegalStateException("Could not serialize callback payload", exc);
    }
  }
}
