// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.idem;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HexFormat;
import java.util.Objects;

/**
 * Provides mission-level idempotency checks and persistence.
 */
public class IdempotencyService {

  private final IdempotencyStore store;
  private final ObjectMapper objectMapper;

  public IdempotencyService(IdempotencyStore store, ObjectMapper objectMapper) {
    this.store = Objects.requireNonNull(store, "store");
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
  }

  public IdempotencyResult check(String bizType, String bizKey, Object request) {
    String requestHash = sha256Hex(toJson(request));
    return store.findByBizTypeAndBizKey(bizType, bizKey)
        .map(record -> {
          if (!record.requestHash().equals(requestHash)) {
            throw new IdempotencyConflictException(
                "Idempotency key reused with different request payload"
            );
          }
          return new IdempotencyResult(true, record.responseJson());
        })
        .orElseGet(() -> new IdempotencyResult(false, null));
  }

  public void storeSuccess(
      String bizType,
      String bizKey,
      Object request,
      Object response
  ) {
    store.save(
        new IdempotencyRecord(
            bizType,
            bizKey,
            sha256Hex(toJson(request)),
            toJson(response),
            "SUCCESS"
        )
    );
  }

  private String toJson(Object value) {
    try {
      return objectMapper.writeValueAsString(value);
    }
    catch (JsonProcessingException exc) {
      throw new IllegalStateException("Could not serialize payload to JSON", exc);
    }
  }

  private String sha256Hex(String source) {
    try {
      MessageDigest digest = MessageDigest.getInstance("SHA-256");
      byte[] hashBytes = digest.digest(source.getBytes(StandardCharsets.UTF_8));
      return HexFormat.of().formatHex(hashBytes);
    }
    catch (NoSuchAlgorithmException exc) {
      throw new IllegalStateException("SHA-256 not available", exc);
    }
  }
}
