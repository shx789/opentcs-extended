// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * Response payload for WCS task query.
 */
public record QueryWcsTaskResp(
    @JsonProperty("biz_task_no")
    String bizTaskNo,
    @JsonProperty("task_type")
    String taskType,
    @JsonProperty("mission_no")
    String missionNo,
    @JsonProperty("task_no")
    String taskNo,
    @JsonProperty("plc_job_no")
    String plcJobNo,
    @JsonProperty("rcs_status")
    String rcsStatus,
    @JsonProperty("trace_id")
    String traceId,
    @JsonProperty("request_id")
    String requestId,
    @JsonProperty("created_at")
    String createdAt,
    @JsonProperty("updated_at")
    String updatedAt
) {
}
