// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import java.time.Instant;
import java.util.Objects;
import java.util.Optional;
import org.opentcs.rcs.api.dto.AgvEventCallbackReq;
import org.opentcs.rcs.api.wcs.WmsTaskResultService;
import org.opentcs.rcs.callback.CallbackOutboxService;
import org.opentcs.rcs.core.mission.MissionCallbackTarget;
import org.opentcs.rcs.core.mission.MissionStore;
import org.opentcs.rcs.core.task.TaskStore;
import org.opentcs.rcs.core.task.WcsTaskRecord;
import org.opentcs.rcs.core.task.WcsTaskStatus;
import org.opentcs.rcs.core.task.WcsTaskStatusTransitions;
import org.opentcs.rcs.core.task.WcsTaskType;
import org.opentcs.rcs.http.RequestContext;

/**
 * Consumes openTCS events and enqueues mapped WCS callbacks.
 */
public class OpenTcsSseEventConsumer {

  private final OpenTcsEventProjector eventProjector;
  private final CallbackOutboxService callbackOutboxService;
  private final MissionStore missionStore;
  private final TaskStore taskStore;
  private final WmsTaskResultService wmsTaskResultService;

  public OpenTcsSseEventConsumer(
      OpenTcsEventProjector eventProjector,
      CallbackOutboxService callbackOutboxService,
      MissionStore missionStore,
      TaskStore taskStore,
      WmsTaskResultService wmsTaskResultService
  ) {
    this.eventProjector = Objects.requireNonNull(eventProjector, "eventProjector");
    this.callbackOutboxService = Objects.requireNonNull(callbackOutboxService, "callbackOutboxService");
    this.missionStore = Objects.requireNonNull(missionStore, "missionStore");
    this.taskStore = Objects.requireNonNull(taskStore, "taskStore");
    this.wmsTaskResultService = Objects.requireNonNull(wmsTaskResultService, "wmsTaskResultService");
  }

  public Optional<AgvEventCallbackReq> consume(
      OpenTcsTransportOrderEvent event,
      RequestContext requestContext
  ) {
    Optional<MissionCallbackTarget> missionTargetOpt = missionStore.findByMissionNo(event.missionNo());
    if (missionTargetOpt.isEmpty()) {
      return Optional.empty();
    }

    MissionCallbackTarget missionTarget = missionTargetOpt.orElseThrow();
    RequestContext effectiveContext = resolveContext(requestContext, missionTarget);
    Optional<AgvEventCallbackReq> mappedEvent = eventProjector.toWcsEvent(event, effectiveContext);
    mappedEvent.ifPresent(callback -> {
      callbackOutboxService.enqueue(
          callback.missionNo(),
          missionTarget.callbackUrl(),
          callback,
          buildIdemKey(callback)
      );
      missionStore.updateStatus(callback.missionNo(), missionStatusOf(callback.eventType()));
      projectTaskStateAndReportResult(event, callback, effectiveContext);
    });
    return mappedEvent;
  }

  private RequestContext resolveContext(
      RequestContext requestContext,
      MissionCallbackTarget missionTarget
  ) {
    if ("legacy".equals(missionTarget.traceId()) || "legacy".equals(missionTarget.requestId())) {
      return requestContext;
    }
    return new RequestContext(missionTarget.traceId(), missionTarget.requestId());
  }

  private String buildIdemKey(AgvEventCallbackReq callback) {
    return callback.missionNo() + "|" + callback.eventType() + "|" + callback.eventTime();
  }

  private String missionStatusOf(String eventType) {
    return switch (eventType) {
      case "ARRIVED_FROM", "ARRIVED_TO", "PICKED" -> "IN_PROGRESS";
      case "DROPPED" -> "DONE";
      case "FAILED" -> "FAILED";
      default -> "IN_PROGRESS";
    };
  }

  private void projectTaskStateAndReportResult(
      OpenTcsTransportOrderEvent event,
      AgvEventCallbackReq callback,
      RequestContext requestContext
  ) {
    taskStore.findByMissionNo(event.missionNo()).ifPresent(task -> {
      WcsTaskStatus targetStatus = mapTaskStatus(task.taskType(), callback.eventType());
      if (targetStatus == null || task.rcsStatus() == targetStatus) {
        return;
      }
      if (!WcsTaskStatusTransitions.canTransit(task.rcsStatus(), targetStatus)) {
        return;
      }
      WcsTaskRecord updated = task.withStatus(targetStatus, Instant.now().toString());
      taskStore.save(updated);
      wmsTaskResultService.enqueueResultIfConfigured(
          updated,
          targetStatus,
          requestContext,
          callback.reasonCode(),
          callback.reasonMsg(),
          callback.eventTime()
      );
    });
  }

  private WcsTaskStatus mapTaskStatus(WcsTaskType taskType, String eventType) {
    return switch (eventType) {
      case "ARRIVED_FROM", "ARRIVED_TO", "PICKED" -> WcsTaskStatus.IN_PROGRESS;
      case "DROPPED" -> taskType == WcsTaskType.INBOUND ? WcsTaskStatus.WAIT_PLC : WcsTaskStatus.DONE;
      case "FAILED" -> WcsTaskStatus.FAILED;
      default -> null;
    };
  }
}
