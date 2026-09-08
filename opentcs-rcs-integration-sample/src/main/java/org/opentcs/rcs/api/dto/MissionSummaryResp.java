// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * Mission summary for monitor/list APIs.
 */
public record MissionSummaryResp(
    @JsonProperty("mission_no")
    String missionNo,
    @JsonProperty("task_no")
    String taskNo,
    @JsonProperty("rcs_status")
    String rcsStatus,
    @JsonProperty("from_point")
    String fromPoint,
    @JsonProperty("to_point")
    String toPoint,
    @JsonProperty("pallet_no")
    String palletNo,
    Integer priority
) {
}
