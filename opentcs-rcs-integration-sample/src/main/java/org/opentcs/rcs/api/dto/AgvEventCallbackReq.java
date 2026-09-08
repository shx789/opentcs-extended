// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * Callback payload for I-10 AGV events.
 */
public record AgvEventCallbackReq(
    @JsonProperty("mission_no")
    String missionNo,
    @JsonProperty("task_no")
    String taskNo,
    @JsonProperty("event_type")
    String eventType,
    @JsonProperty("point_id")
    String pointId,
    @JsonProperty("agv_id")
    String agvId,
    @JsonProperty("battery")
    Integer battery,
    @JsonProperty("reason_code")
    String reasonCode,
    @JsonProperty("reason_msg")
    String reasonMsg,
    @JsonProperty("trace_id")
    String traceId,
    @JsonProperty("request_id")
    String requestId,
    @JsonProperty("event_time")
    String eventTime
) {
}
