// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsTransportOrderReq;

/**
 * Port for creating transport orders in openTCS.
 */
public interface OpenTcsOrderClient {

  void createTransportOrder(String orderName, OpenTcsTransportOrderReq payload);

  default void cancelTransportOrder(String orderName) {
  }
}
