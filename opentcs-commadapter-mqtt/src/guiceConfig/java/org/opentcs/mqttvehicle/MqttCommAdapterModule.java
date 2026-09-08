// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.mqttvehicle;

import com.google.inject.assistedinject.FactoryModuleBuilder;
import org.opentcs.customizations.kernel.KernelInjectionModule;

/**
 * Configures/binds MQTT AGV communication adapters.
 */
public class MqttCommAdapterModule
    extends
      KernelInjectionModule {

  /**
   * Creates a new instance.
   */
  public MqttCommAdapterModule() {
  }

  @Override
  protected void configure() {
    install(new FactoryModuleBuilder().build(MqttCommAdapterComponentsFactory.class));
    vehicleCommAdaptersBinder().addBinding().to(MqttCommAdapterFactory.class);
  }
}
