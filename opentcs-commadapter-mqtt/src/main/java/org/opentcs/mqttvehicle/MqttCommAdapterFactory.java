// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.mqttvehicle;

import static java.util.Objects.requireNonNull;

import jakarta.inject.Inject;
import org.opentcs.data.model.Vehicle;
import org.opentcs.drivers.vehicle.VehicleCommAdapterDescription;
import org.opentcs.drivers.vehicle.VehicleCommAdapterFactory;

/**
 * A factory for MQTT AGV communication adapters.
 */
public class MqttCommAdapterFactory
    implements
      VehicleCommAdapterFactory {

  private final MqttCommAdapterComponentsFactory adapterFactory;
  private boolean initialized;

  /**
   * Creates a new factory.
   *
   * @param adapterFactory The adapter components factory.
   */
  @Inject
  public MqttCommAdapterFactory(MqttCommAdapterComponentsFactory adapterFactory) {
    this.adapterFactory = requireNonNull(adapterFactory, "adapterFactory");
  }

  @Override
  public void initialize() {
    if (isInitialized()) {
      return;
    }
    initialized = true;
  }

  @Override
  public boolean isInitialized() {
    return initialized;
  }

  @Override
  public void terminate() {
    if (!isInitialized()) {
      return;
    }
    initialized = false;
  }

  @Override
  public VehicleCommAdapterDescription getDescription() {
    return new MqttCommAdapterDescription();
  }

  @Override
  public boolean providesAdapterFor(Vehicle vehicle) {
    requireNonNull(vehicle, "vehicle");
    return true;
  }

  @Override
  public MqttCommAdapter getAdapterFor(Vehicle vehicle) {
    requireNonNull(vehicle, "vehicle");
    return adapterFactory.createMqttCommAdapter(vehicle);
  }
}
