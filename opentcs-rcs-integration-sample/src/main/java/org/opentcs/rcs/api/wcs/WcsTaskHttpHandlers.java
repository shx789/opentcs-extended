// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

import com.fasterxml.jackson.databind.ObjectMapper;
import io.javalin.http.Context;
import io.javalin.http.Handler;
import java.util.Objects;
import org.opentcs.rcs.api.dto.ApiResponse;
import org.opentcs.rcs.api.dto.CancelWcsTaskResp;
import org.opentcs.rcs.api.dto.CreateWcsTaskReq;
import org.opentcs.rcs.api.dto.CreateWcsTaskResp;
import org.opentcs.rcs.api.dto.QueryWcsTaskResp;
import org.opentcs.rcs.http.RequestContext;

/**
 * HTTP handlers for WMS-facing task endpoints.
 */
public final class WcsTaskHttpHandlers {

  private WcsTaskHttpHandlers() {
  }

  public static Handler createInboundTaskHandler(
      WcsTaskService taskService,
      ObjectMapper objectMapper
  ) {
    Objects.requireNonNull(taskService, "taskService");
    Objects.requireNonNull(objectMapper, "objectMapper");
    return ctx -> handleCreateInboundTask(ctx, taskService, objectMapper);
  }

  public static Handler createOutboundTaskHandler(
      WcsTaskService taskService,
      ObjectMapper objectMapper
  ) {
    Objects.requireNonNull(taskService, "taskService");
    Objects.requireNonNull(objectMapper, "objectMapper");
    return ctx -> handleCreateOutboundTask(ctx, taskService, objectMapper);
  }

  public static Handler queryTaskHandler(WcsTaskService taskService) {
    Objects.requireNonNull(taskService, "taskService");
    return ctx -> handleQueryTask(ctx, taskService);
  }

  public static Handler cancelTaskHandler(WcsTaskService taskService) {
    Objects.requireNonNull(taskService, "taskService");
    return ctx -> handleCancelTask(ctx, taskService);
  }

  private static void handleCreateInboundTask(
      Context ctx,
      WcsTaskService taskService,
      ObjectMapper objectMapper
  )
      throws Exception {
    RequestContext requestContext = RequestContext.from(ctx);
    requestContext.writeToResponse(ctx);
    CreateWcsTaskReq request = objectMapper.readValue(ctx.body(), CreateWcsTaskReq.class);
    CreateWcsTaskResp response = taskService.createInboundTask(request, requestContext);
    ctx.status(200).json(ApiResponse.success(response));
  }

  private static void handleCreateOutboundTask(
      Context ctx,
      WcsTaskService taskService,
      ObjectMapper objectMapper
  )
      throws Exception {
    RequestContext requestContext = RequestContext.from(ctx);
    requestContext.writeToResponse(ctx);
    CreateWcsTaskReq request = objectMapper.readValue(ctx.body(), CreateWcsTaskReq.class);
    CreateWcsTaskResp response = taskService.createOutboundTask(request, requestContext);
    ctx.status(200).json(ApiResponse.success(response));
  }

  private static void handleQueryTask(
      Context ctx,
      WcsTaskService taskService
  ) {
    RequestContext.from(ctx).writeToResponse(ctx);
    String bizTaskNo = ctx.pathParam("biz_task_no");
    QueryWcsTaskResp response = taskService.queryTask(bizTaskNo);
    ctx.status(200).json(ApiResponse.success(response));
  }

  private static void handleCancelTask(
      Context ctx,
      WcsTaskService taskService
  ) {
    RequestContext.from(ctx).writeToResponse(ctx);
    String bizTaskNo = ctx.pathParam("biz_task_no");
    CancelWcsTaskResp response = taskService.cancelTask(bizTaskNo);
    ctx.status(200).json(ApiResponse.success(response));
  }
}
