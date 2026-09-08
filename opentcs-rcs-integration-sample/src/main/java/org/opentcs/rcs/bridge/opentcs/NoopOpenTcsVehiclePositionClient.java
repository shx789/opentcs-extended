// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

/**
 * A position client that intentionally does not update openTCS.
 */
public class NoopOpenTcsVehiclePositionClient
    implements
      OpenTcsVehiclePositionClient {

  public NoopOpenTcsVehiclePositionClient() {
  }

  @Override
  public void updateVehiclePosition(String vehicleName, String pointName) {
  }
}
