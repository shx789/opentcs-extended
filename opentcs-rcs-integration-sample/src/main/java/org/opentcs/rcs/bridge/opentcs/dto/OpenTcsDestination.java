// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs.dto;

import java.util.List;

/**
 * openTCS destination item.
 */
public record OpenTcsDestination(
    String locationName,
    String operation,
    List<OpenTcsProperty> properties
) {
}
