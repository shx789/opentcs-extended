// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.idem;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;

/**
 * Result of idempotency pre-check.
 */
public record IdempotencyResult(
    boolean hit,
    String responseJson
) {

  public <T> T cachedResponse(ObjectMapper objectMapper, Class<T> targetType) {
    try {
      return objectMapper.readValue(responseJson, targetType);
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not deserialize cached response", exc);
    }
  }
}
