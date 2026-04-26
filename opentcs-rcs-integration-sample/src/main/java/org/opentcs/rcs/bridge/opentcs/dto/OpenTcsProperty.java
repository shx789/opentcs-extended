// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs.dto;

/**
 * openTCS property key/value entry.
 */
public record OpenTcsProperty(
    String key,
    String value
) {
}
