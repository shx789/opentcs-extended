// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv;

import java.time.Instant;

/**
 * Normalized AGV MQTT status/task feedback message.
 */
public record AgvMqttStatusMessage(
    String messageId,
    String agvId,
    String eventType,
    String missionNo,
    String taskNo,
    String pointId,
    Integer battery,
    Boolean online,
    Double rosX,
    Double rosY,
    Double rosYaw,
    String errorCode,
    String errorMsg,
    Long sequence,
    Instant eventTime
) {
}
