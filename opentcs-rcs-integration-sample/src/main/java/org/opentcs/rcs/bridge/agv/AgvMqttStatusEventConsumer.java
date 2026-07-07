// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv;

import java.time.Instant;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
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
 * Consumes AGV MQTT feedback and projects task events to WCS callbacks.
 */
public class AgvMqttStatusEventConsumer {

  private static final DateTimeFormatter EVENT_TIME_FORMATTER
      = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss").withZone(ZoneOffset.UTC);

  private final CallbackOutboxService callbackOutboxService;
  private final MissionStore missionStore;
  private final TaskStore taskStore;
  private final WmsTaskResultService wmsTaskResultService;
  private final AgvVehiclePositionSynchronizer vehiclePositionSynchronizer;

  public AgvMqttStatusEventConsumer(
      CallbackOutboxService callbackOutboxService,
      MissionStore missionStore,
      TaskStore taskStore,
      WmsTaskResultService wmsTaskResultService,
      AgvVehiclePositionSynchronizer vehiclePositionSynchronizer
  ) {
    this.callbackOutboxService = Objects.requireNonNull(
        callbackOutboxService,
        "callbackOutboxService"
    );
    this.missionStore = Objects.requireNonNull(missionStore, "missionStore");
    this.taskStore = Objects.requireNonNull(taskStore, "taskStore");
    this.wmsTaskResultService = Objects.requireNonNull(
        wmsTaskResultService,
        "wmsTaskResultService"
    );
    this.vehiclePositionSynchronizer = Objects.requireNonNull(
        vehiclePositionSynchronizer,
        "vehiclePositionSynchronizer"
    );
  }

  public Optional<AgvEventCallbackReq> consume(
      AgvMqttStatusMessage message,
      RequestContext requestContext
  ) {
    Objects.requireNonNull(message, "message");
    Objects.requireNonNull(requestContext, "requestContext");
    vehiclePositionSynchronizer.synchronize(message);
    if (message.missionNo() == null || message.missionNo().isBlank()) {
      return Optional.empty();
    }

    Optional<MissionCallbackTarget> missionTargetOpt
        = missionStore.findByMissionNo(message.missionNo());
    if (missionTargetOpt.isEmpty()) {
      return Optional.empty();
    }

    Optional<String> callbackEventType = callbackEventType(message);
    if (callbackEventType.isEmpty()) {
      return Optional.empty();
    }

    MissionCallbackTarget missionTarget = missionTargetOpt.orElseThrow();
    RequestContext effectiveContext = resolveContext(requestContext, missionTarget);
    AgvEventCallbackReq callback = buildCallback(
        message,
        callbackEventType.orElseThrow(),
        missionTarget,
        effectiveContext
    );
    callbackOutboxService.enqueue(
        callback.missionNo(),
        missionTarget.callbackUrl(),
        callback,
        idempotencyKey(message, callback.eventType())
    );
    missionStore.updateStatus(callback.missionNo(), missionStatusOf(callback.eventType()));
    projectTaskStateAndReportResult(message, callback, effectiveContext);
    return Optional.of(callback);
  }

  private Optional<String> callbackEventType(AgvMqttStatusMessage message) {
    return switch (message.eventType()) {
      case "ARRIVED_FROM", "PICKED", "ARRIVED_TO", "DROPPED", "FAILED" -> Optional.of(
          message.eventType()
      );
      case "COMPLETED", "COMPLETE", "FINISHED" -> Optional.of("DROPPED");
      case "ERROR", "EXCEPTION", "FAULT" -> Optional.of("FAILED");
      case "ARRIVED" -> deriveArrivedEventType(message);
      default -> Optional.empty();
    };
  }

  private Optional<String> deriveArrivedEventType(AgvMqttStatusMessage message) {
    if (message.pointId() == null) {
      return Optional.empty();
    }
    Optional<String> taskEventType
        = taskStore.findByMissionNo(message.missionNo()).flatMap(task -> {
      if (message.pointId().equals(task.fromPoint())) {
        return Optional.of("ARRIVED_FROM");
      }
      if (message.pointId().equals(task.toPoint())) {
        return Optional.of("ARRIVED_TO");
      }
      return Optional.empty();
    });
    if (taskEventType.isPresent()) {
      return taskEventType;
    }
    return missionStore.findByMissionNo(message.missionNo()).flatMap(target -> {
      if (message.pointId().equals(target.fromPoint())) {
        return Optional.of("ARRIVED_FROM");
      }
      if (message.pointId().equals(target.toPoint())) {
        return Optional.of("ARRIVED_TO");
      }
      return Optional.empty();
    });
  }

  private AgvEventCallbackReq buildCallback(
      AgvMqttStatusMessage message,
      String eventType,
      MissionCallbackTarget missionTarget,
      RequestContext requestContext
  ) {
    return new AgvEventCallbackReq(
        message.missionNo(),
        message.taskNo() == null ? missionTarget.taskNo() : message.taskNo(),
        eventType,
        message.pointId(),
        message.agvId(),
        message.battery(),
        reasonCode(message, eventType),
        reasonMsg(message, eventType),
        requestContext.traceId(),
        requestContext.requestId(),
        EVENT_TIME_FORMATTER.format(message.eventTime())
    );
  }

  private String reasonCode(AgvMqttStatusMessage message, String eventType) {
    if (!"FAILED".equals(eventType)) {
      return null;
    }
    return message.errorCode() == null ? "AGV_ERROR" : message.errorCode();
  }

  private String reasonMsg(AgvMqttStatusMessage message, String eventType) {
    if (!"FAILED".equals(eventType)) {
      return null;
    }
    return message.errorMsg() == null ? "AGV reported task failure" : message.errorMsg();
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

  private String idempotencyKey(AgvMqttStatusMessage message, String eventType) {
    if (message.messageId() != null && !message.messageId().isBlank()) {
      return message.messageId();
    }
    String sequence = message.sequence() == null ? "-" : message.sequence().toString();
    return message.agvId() + "|" + message.missionNo() + "|" + eventType + "|"
        + message.eventTime() + "|" + sequence;
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
      AgvMqttStatusMessage message,
      AgvEventCallbackReq callback,
      RequestContext requestContext
  ) {
    taskStore.findByMissionNo(message.missionNo()).ifPresent(task -> {
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
      case "DROPPED" -> taskType == WcsTaskType.INBOUND
          ? WcsTaskStatus.WAIT_PLC
          : WcsTaskStatus.DONE;
      case "FAILED" -> WcsTaskStatus.FAILED;
      default -> null;
    };
  }
}
