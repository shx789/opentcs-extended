// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.mqttvehicle;

import org.opentcs.data.model.Vehicle;

/**
 * A factory for MQTT AGV specific instances.
 */
public interface MqttCommAdapterComponentsFactory {

  /**
   * Creates a new communication adapter for the given vehicle.
   *
   * @param vehicle The vehicle.
   * @return A new communication adapter.
   */
  MqttCommAdapter createMqttCommAdapter(Vehicle vehicle);
}
