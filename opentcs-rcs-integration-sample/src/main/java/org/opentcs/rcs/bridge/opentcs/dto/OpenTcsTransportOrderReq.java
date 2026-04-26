// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs.dto;

import java.util.List;

/**
 * openTCS transport order creation payload.
 */
public record OpenTcsTransportOrderReq(
    boolean incompleteName,
    String type,
    String deadline,
    List<OpenTcsDestination> destinations,
    List<OpenTcsProperty> properties
) {
}
