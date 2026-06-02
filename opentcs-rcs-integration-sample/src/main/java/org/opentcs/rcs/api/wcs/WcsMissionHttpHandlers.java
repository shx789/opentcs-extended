// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

import com.fasterxml.jackson.databind.ObjectMapper;
import io.javalin.http.Context;
import io.javalin.http.Handler;
import java.util.Objects;
import org.opentcs.rcs.api.dto.ApiResponse;
import org.opentcs.rcs.api.dto.CancelMissionResp;
import org.opentcs.rcs.api.dto.CreateMissionReq;
import org.opentcs.rcs.api.dto.CreateMissionResp;
import org.opentcs.rcs.api.dto.QueryMissionResp;
import org.opentcs.rcs.http.RequestContext;

/**
 * HTTP handlers for WCS-facing mission endpoints.
 */
public final class WcsMissionHttpHandlers {

  private WcsMissionHttpHandlers() {
  }

  public static Handler createMissionHandler(
      WcsMissionService missionService,
      ObjectMapper objectMapper
  ) {
    Objects.requireNonNull(missionService, "missionService");
    Objects.requireNonNull(objectMapper, "objectMapper");
    return ctx -> handleCreateMission(ctx, missionService, objectMapper);
  }

  public static Handler cancelMissionHandler(WcsMissionService missionService) {
    Objects.requireNonNull(missionService, "missionService");
    return ctx -> handleCancelMission(ctx, missionService);
  }

  public static Handler queryMissionHandler(WcsMissionService missionService) {
    Objects.requireNonNull(missionService, "missionService");
    return ctx -> handleQueryMission(ctx, missionService);
  }

  public static Handler listMissionsHandler(WcsMissionService missionService) {
    Objects.requireNonNull(missionService, "missionService");
    return ctx -> handleListMissions(ctx, missionService);
  }

  private static void handleCreateMission(
      Context ctx,
      WcsMissionService missionService,
      ObjectMapper objectMapper
  ) throws Exception {
    RequestContext requestContext = RequestContext.from(ctx);
    requestContext.writeToResponse(ctx);
    CreateMissionReq request = objectMapper.readValue(ctx.body(), CreateMissionReq.class);
    CreateMissionResp response = missionService.createMission(request, requestContext);
    ctx.status(200).json(ApiResponse.success(response));
  }

  private static void handleCancelMission(
      Context ctx,
      WcsMissionService missionService
  ) {
    RequestContext.from(ctx).writeToResponse(ctx);
    String missionNo = ctx.pathParam("mission_no");
    CancelMissionResp response = missionService.cancelMission(missionNo);
    ctx.status(200).json(ApiResponse.success(response));
  }

  private static void handleQueryMission(
      Context ctx,
      WcsMissionService missionService
  ) {
    RequestContext.from(ctx).writeToResponse(ctx);
    String missionNo = ctx.pathParam("mission_no");
    QueryMissionResp response = missionService.queryMission(missionNo);
    ctx.status(200).json(ApiResponse.success(response));
  }

  private static void handleListMissions(
      Context ctx,
      WcsMissionService missionService
  ) {
    RequestContext.from(ctx).writeToResponse(ctx);
    ctx.status(200).json(ApiResponse.success(missionService.listMissions()));
  }
}
