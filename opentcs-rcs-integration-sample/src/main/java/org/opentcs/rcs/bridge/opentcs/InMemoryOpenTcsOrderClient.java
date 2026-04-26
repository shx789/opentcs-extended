// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsTransportOrderReq;

/**
 * In-memory openTCS order client implementation for local runs/tests.
 */
public class InMemoryOpenTcsOrderClient
    implements OpenTcsOrderClient {

  private final Map<String, OpenTcsTransportOrderReq> orders = new ConcurrentHashMap<>();

  @Override
  public void createTransportOrder(String orderName, OpenTcsTransportOrderReq payload) {
    orders.put(orderName, payload);
  }

  @Override
  public void cancelTransportOrder(String orderName) {
    orders.remove(orderName);
  }

  public Map<String, OpenTcsTransportOrderReq> createdOrders() {
    return orders;
  }
}
