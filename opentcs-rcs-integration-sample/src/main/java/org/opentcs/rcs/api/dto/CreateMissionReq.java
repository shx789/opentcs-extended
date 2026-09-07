// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.dto;

import com.fasterxml.jackson.annotation.JsonAlias;
import com.fasterxml.jackson.annotation.JsonProperty;

/**
 * Request payload for creating a mission from WCS.
 */
public record CreateMissionReq(
    @JsonProperty("mission_no")
    @JsonAlias("missionNo")
    String missionNo,
    @JsonProperty("task_no")
    @JsonAlias("taskNo")
    String taskNo,
    @JsonProperty("from_point")
    @JsonAlias("fromPoint")
    String fromPoint,
    @JsonProperty("to_point")
    @JsonAlias("toPoint")
    String toPoint,
    @JsonProperty("pallet_no")
    @JsonAlias("palletNo")
    String palletNo,
    @JsonProperty("priority")
    Integer priority,
    @JsonProperty("callback_url")
    @JsonAlias("callbackUrl")
    String callbackUrl,
    @JsonProperty("mission_type")
    @JsonAlias("missionType")
    String missionType,
    @JsonProperty("from_operation")
    @JsonAlias("fromOperation")
    String fromOperation,
    @JsonProperty("to_operation")
    @JsonAlias("toOperation")
    String toOperation
) {
  public CreateMissionReq(
      String missionNo,
      String taskNo,
      String fromPoint,
      String toPoint,
      String palletNo,
      Integer priority,
      String callbackUrl
  ) {
    this(
        missionNo,
        taskNo,
        fromPoint,
        toPoint,
        palletNo,
        priority,
        callbackUrl,
        null,
        null,
        null
    );
  }
}
