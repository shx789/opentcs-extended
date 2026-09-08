// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.kernel.extensions.servicewebapi.v1.binding;

import static java.util.Objects.requireNonNull;

import com.fasterxml.jackson.annotation.JsonCreator;
import com.fasterxml.jackson.annotation.JsonProperty;
import jakarta.annotation.Nonnull;

/**
 * A request for updating a vehicle's current point position.
 */
public class PutVehiclePositionRequestTO {

  private final String pointName;

  @JsonCreator
  public PutVehiclePositionRequestTO(
      @Nonnull
      @JsonProperty(value = "pointName", required = true)
      String pointName
  ) {
    this.pointName = requireNonNull(pointName, "pointName");
  }

  @Nonnull
  public String getPointName() {
    return pointName;
  }
}
