// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * Callback payload for WCS -> WMS task result reporting.
 */
public record WmsTaskResultCallbackReq(
    @JsonProperty("biz_task_no")
    String bizTaskNo,
    @JsonProperty("task_type")
    String taskType,
    @JsonProperty("result_type")
    String resultType,
    @JsonProperty("mission_no")
    String missionNo,
    @JsonProperty("task_no")
    String taskNo,
    @JsonProperty("plc_job_no")
    String plcJobNo,
    @JsonProperty("rcs_status")
    String rcsStatus,
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
