// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.dto;

import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * Response payload for mission cancellation.
 */
public record CancelMissionResp(
    @JsonProperty("mission_no")
    String missionNo,
    @JsonProperty("task_no")
    String taskNo,
    @JsonProperty("rcs_status")
    String rcsStatus,
    @JsonProperty("idem_hit")
    boolean idemHit
) {
}
