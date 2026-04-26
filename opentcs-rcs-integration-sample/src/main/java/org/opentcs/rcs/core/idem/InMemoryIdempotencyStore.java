// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.idem;

import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

/**
 * In-memory idempotency storage for local runs/tests.
 */
public class InMemoryIdempotencyStore
    implements IdempotencyStore {

  private final ConcurrentMap<String, IdempotencyRecord> records = new ConcurrentHashMap<>();

  @Override
  public Optional<IdempotencyRecord> findByBizTypeAndBizKey(String bizType, String bizKey) {
    return Optional.ofNullable(records.get(makeKey(bizType, bizKey)));
  }

  @Override
  public void save(IdempotencyRecord record) {
    records.put(makeKey(record.bizType(), record.bizKey()), record);
  }

  private String makeKey(String bizType, String bizKey) {
    return bizType + "::" + bizKey;
  }
}
