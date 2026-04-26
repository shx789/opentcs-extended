// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import java.util.Objects;

/**
 * Signals an unsuccessful interaction with openTCS HTTP APIs.
 */
public class OpenTcsClientException
    extends RuntimeException {

  public OpenTcsClientException(String message) {
    super(Objects.requireNonNull(message, "message"));
  }

  public OpenTcsClientException(String message, Throwable cause) {
    super(Objects.requireNonNull(message, "message"), cause);
  }
}
