// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import java.time.Instant;

/**
 * Normalized openTCS transport-order event for projection.
 */
public record OpenTcsTransportOrderEvent(
    String missionNo,
    String taskNo,
    String transportOrderState,
    Integer currentDriveOrderIndex,
    String currentPointId,
    String processingVehicle,
    String failureReason,
    Instant eventTime
) {
}
