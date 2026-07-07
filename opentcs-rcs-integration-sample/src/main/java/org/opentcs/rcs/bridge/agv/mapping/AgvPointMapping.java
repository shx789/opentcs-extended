// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv.mapping;

/**
 * Mapping data for a point shared by AGV/ROS and openTCS.
 */
public record AgvPointMapping(
    String pointName,
    double rosX,
    double rosY,
    double rosYaw
) {
}
