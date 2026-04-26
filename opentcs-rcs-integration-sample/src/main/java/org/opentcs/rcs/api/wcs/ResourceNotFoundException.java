// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

/**
 * Exception for not-found business resources.
 */
public class ResourceNotFoundException
    extends IllegalArgumentException {

  public ResourceNotFoundException(String message) {
    super(message);
  }
}
