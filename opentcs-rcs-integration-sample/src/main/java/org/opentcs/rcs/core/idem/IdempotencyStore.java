// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.idem;

import java.util.Optional;

/**
 * Storage abstraction for idempotency records.
 */
public interface IdempotencyStore {

  Optional<IdempotencyRecord> findByBizTypeAndBizKey(String bizType, String bizKey);

  void save(IdempotencyRecord record);
}
