// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * Response payload for mission query.
 */
public record QueryMissionResp(
    @JsonProperty("mission_no")
    String missionNo,
    @JsonProperty("task_no")
    String taskNo,
    @JsonProperty("rcs_status")
    String rcsStatus
) {
}
