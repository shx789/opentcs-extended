// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.mqttvehicle;

import org.opentcs.drivers.vehicle.VehicleCommAdapterDescription;

/**
 * Description for the MQTT AGV communication adapter.
 */
public class MqttCommAdapterDescription
    extends
      VehicleCommAdapterDescription {

  /**
   * Creates a new instance.
   */
  public MqttCommAdapterDescription() {
  }

  @Override
  public String getDescription() {
    return "MQTT AGV Adapter";
  }

  @Override
  public boolean isSimVehicleCommAdapter() {
    return false;
  }
}
