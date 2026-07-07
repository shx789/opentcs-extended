// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

/**
 * Updates a vehicle's current position in openTCS.
 */
public interface OpenTcsVehiclePositionClient {

  void updateVehiclePosition(String vehicleName, String pointName);
}
