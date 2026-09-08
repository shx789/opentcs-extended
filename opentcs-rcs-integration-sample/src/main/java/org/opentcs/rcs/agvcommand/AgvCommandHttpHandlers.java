// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.agvcommand;

import io.javalin.http.Handler;
import java.util.List;
import java.util.Objects;
import org.opentcs.rcs.api.dto.ApiResponse;
import org.opentcs.rcs.http.RequestContext;

/**
 * HTTP handlers exposing AGV command outbox state.
 */
public final class AgvCommandHttpHandlers {

  private AgvCommandHttpHandlers() {
  }

  public static Handler queryMissionCommandsHandler(AgvCommandOutboxStore store) {
    Objects.requireNonNull(store, "store");
    return ctx -> {
      RequestContext.from(ctx).writeToResponse(ctx);
      String missionNo = ctx.pathParam("mission_no");
      List<AgvCommandStatusResp> commands = store.findByMissionNo(missionNo).stream()
          .map(AgvCommandStatusResp::from)
          .toList();
      ctx.status(200).json(ApiResponse.success(commands));
    };
  }
}
