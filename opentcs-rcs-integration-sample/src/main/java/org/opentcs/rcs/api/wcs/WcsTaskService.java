// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.time.Instant;
import java.util.Objects;
import java.util.Set;
import org.opentcs.rcs.api.dto.CancelMissionResp;
import org.opentcs.rcs.api.dto.CancelWcsTaskResp;
import org.opentcs.rcs.api.dto.CreateMissionReq;
import org.opentcs.rcs.api.dto.CreateMissionResp;
import org.opentcs.rcs.api.dto.CreateWcsTaskReq;
import org.opentcs.rcs.api.dto.CreateWcsTaskResp;
import org.opentcs.rcs.api.dto.QueryMissionResp;
import org.opentcs.rcs.api.dto.QueryWcsTaskResp;
import org.opentcs.rcs.core.idem.IdempotencyResult;
import org.opentcs.rcs.core.idem.IdempotencyService;
import org.opentcs.rcs.core.task.TaskStore;
import org.opentcs.rcs.core.task.WcsTaskRecord;
import org.opentcs.rcs.core.task.WcsTaskStatus;
import org.opentcs.rcs.core.task.WcsTaskStatusTransitions;
import org.opentcs.rcs.core.task.WcsTaskType;
import org.opentcs.rcs.http.RequestContext;

/**
 * WMS-facing task service for inbound/outbound orchestration over mission APIs.
 */
public class WcsTaskService {

  private static final String BIZ_TYPE_INBOUND_CREATE = "WMS_INBOUND_TASK_CREATE";
  private static final String BIZ_TYPE_OUTBOUND_CREATE = "WMS_OUTBOUND_TASK_CREATE";
  private static final Set<WcsTaskStatus> TERMINAL_STATUSES = Set.of(
      WcsTaskStatus.DONE,
      WcsTaskStatus.FAILED,
      WcsTaskStatus.CANCELED
  );

  private final IdempotencyService idempotencyService;
  private final WcsMissionService missionService;
  private final TaskStore taskStore;
  private final ObjectMapper objectMapper;

  public WcsTaskService(
      IdempotencyService idempotencyService,
      WcsMissionService missionService,
      TaskStore taskStore,
      ObjectMapper objectMapper
  ) {
    this.idempotencyService = Objects.requireNonNull(idempotencyService, "idempotencyService");
    this.missionService = Objects.requireNonNull(missionService, "missionService");
    this.taskStore = Objects.requireNonNull(taskStore, "taskStore");
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
  }

  public CreateWcsTaskResp createInboundTask(
      CreateWcsTaskReq request, RequestContext requestContext
  ) {
    return createTask(request, requestContext, WcsTaskType.INBOUND, BIZ_TYPE_INBOUND_CREATE);
  }

  public CreateWcsTaskResp createOutboundTask(
      CreateWcsTaskReq request, RequestContext requestContext
  ) {
    return createTask(request, requestContext, WcsTaskType.OUTBOUND, BIZ_TYPE_OUTBOUND_CREATE);
  }

  public QueryWcsTaskResp queryTask(String bizTaskNo) {
    requireNonBlank(bizTaskNo, "bizTaskNo");
    WcsTaskRecord currentRecord = taskStore.findByBizTaskNo(bizTaskNo)
        .orElseThrow(() -> new ResourceNotFoundException("biz_task_no not found: " + bizTaskNo));
    WcsTaskRecord refreshedRecord = refreshFromMissionStatus(currentRecord);
    return toQueryResponse(refreshedRecord);
  }

  public CancelWcsTaskResp cancelTask(String bizTaskNo) {
    requireNonBlank(bizTaskNo, "bizTaskNo");
    WcsTaskRecord currentRecord = taskStore.findByBizTaskNo(bizTaskNo)
        .orElseThrow(() -> new ResourceNotFoundException("biz_task_no not found: " + bizTaskNo));

    if (currentRecord.rcsStatus() == WcsTaskStatus.CANCELED) {
      return toCancelResponse(currentRecord, true);
    }
    if (TERMINAL_STATUSES.contains(currentRecord.rcsStatus())) {
      throw new TaskStateConflictException(
          "Task already in terminal status: " + currentRecord.rcsStatus().name()
      );
    }

    CancelMissionResp missionResp = missionService.cancelMission(currentRecord.missionNo());
    WcsTaskRecord canceledRecord = updateTaskStatus(currentRecord, WcsTaskStatus.CANCELED);
    return toCancelResponse(canceledRecord, missionResp.idemHit());
  }

  private CreateWcsTaskResp createTask(
      CreateWcsTaskReq request,
      RequestContext requestContext,
      WcsTaskType taskType,
      String bizType
  ) {
    validateCreateRequest(request);
    Objects.requireNonNull(requestContext, "requestContext");

    IdempotencyResult idemResult = idempotencyService.check(bizType, request.bizTaskNo(), request);
    if (idemResult.hit()) {
      CreateWcsTaskResp cached = idemResult.cachedResponse(objectMapper, CreateWcsTaskResp.class);
      return new CreateWcsTaskResp(
          cached.bizTaskNo(),
          cached.taskType(),
          cached.missionNo(),
          cached.taskNo(),
          cached.plcJobNo(),
          cached.rcsStatus(),
          cached.traceId(),
          cached.requestId(),
          true
      );
    }

    String missionNo = firstNonBlank(
        request.missionNo(), buildDefaultMissionNo(request.bizTaskNo())
    );
    String taskNo = firstNonBlank(request.taskNo(), buildDefaultTaskNo(request.bizTaskNo()));
    CreateMissionResp missionResp = missionService.createMission(
        new CreateMissionReq(
            missionNo,
            taskNo,
            request.fromPoint(),
            request.toPoint(),
            request.palletNo(),
            request.priority(),
            request.callbackUrl()
        ),
        requestContext
    );

    String now = Instant.now().toString();
    WcsTaskRecord record = new WcsTaskRecord(
        request.bizTaskNo(),
        taskType,
        missionResp.missionNo(),
        missionResp.taskNo(),
        null,
        WcsTaskStatus.RECEIVED,
        request.fromPoint(),
        request.toPoint(),
        request.palletNo(),
        request.priority(),
        request.callbackUrl(),
        requestContext.traceId(),
        requestContext.requestId(),
        now,
        now
    );
    taskStore.save(record);

    CreateWcsTaskResp response = toCreateResponse(record, false);
    idempotencyService.storeSuccess(bizType, request.bizTaskNo(), request, response);
    return response;
  }

  private WcsTaskRecord refreshFromMissionStatus(WcsTaskRecord record) {
    QueryMissionResp missionResp = missionService.queryMission(record.missionNo());
    WcsTaskStatus targetStatus = mapMissionStatus(record.taskType(), missionResp.rcsStatus());
    if (record.rcsStatus() == targetStatus) {
      return record;
    }
    return updateTaskStatus(record, targetStatus);
  }

  private WcsTaskRecord updateTaskStatus(WcsTaskRecord record, WcsTaskStatus targetStatus) {
    if (!WcsTaskStatusTransitions.canTransit(record.rcsStatus(), targetStatus)) {
      throw new TaskStateConflictException(
          "Illegal task status transition: " + record.rcsStatus().name() + " -> " + targetStatus
              .name()
      );
    }
    WcsTaskRecord updated = record.withStatus(targetStatus, Instant.now().toString());
    taskStore.save(updated);
    return updated;
  }

  private WcsTaskStatus mapMissionStatus(WcsTaskType taskType, String missionStatus) {
    return switch (missionStatus) {
      case "RECEIVED" -> WcsTaskStatus.RECEIVED;
      case "IN_PROGRESS" -> WcsTaskStatus.IN_PROGRESS;
      case "DONE" -> taskType == WcsTaskType.INBOUND ? WcsTaskStatus.WAIT_PLC : WcsTaskStatus.DONE;
      case "FAILED" -> WcsTaskStatus.FAILED;
      case "CANCELED" -> WcsTaskStatus.CANCELED;
      default -> throw new IllegalArgumentException("Unsupported mission status: " + missionStatus);
    };
  }

  private CreateWcsTaskResp toCreateResponse(WcsTaskRecord record, boolean idemHit) {
    return new CreateWcsTaskResp(
        record.bizTaskNo(),
        record.taskType().name(),
        record.missionNo(),
        record.taskNo(),
        record.plcJobNo(),
        record.rcsStatus().name(),
        record.traceId(),
        record.requestId(),
        idemHit
    );
  }

  private QueryWcsTaskResp toQueryResponse(WcsTaskRecord record) {
    return new QueryWcsTaskResp(
        record.bizTaskNo(),
        record.taskType().name(),
        record.missionNo(),
        record.taskNo(),
        record.plcJobNo(),
        record.rcsStatus().name(),
        record.traceId(),
        record.requestId(),
        record.createdAt(),
        record.updatedAt()
    );
  }

  private CancelWcsTaskResp toCancelResponse(WcsTaskRecord record, boolean idemHit) {
    return new CancelWcsTaskResp(
        record.bizTaskNo(),
        record.taskType().name(),
        record.missionNo(),
        record.taskNo(),
        record.plcJobNo(),
        record.rcsStatus().name(),
        record.traceId(),
        record.requestId(),
        idemHit
    );
  }

  private void validateCreateRequest(CreateWcsTaskReq request) {
    Objects.requireNonNull(request, "request");
    requireNonBlank(request.bizTaskNo(), "bizTaskNo");
    requireNonBlank(request.fromPoint(), "fromPoint");
    requireNonBlank(request.toPoint(), "toPoint");
    requireNonBlank(request.palletNo(), "palletNo");
    requireNonBlank(request.callbackUrl(), "callbackUrl");
  }

  private String buildDefaultMissionNo(String bizTaskNo) {
    return "M_" + normalizeForIdentifier(bizTaskNo);
  }

  private String buildDefaultTaskNo(String bizTaskNo) {
    return "T_" + normalizeForIdentifier(bizTaskNo);
  }

  private String normalizeForIdentifier(String value) {
    return value.trim().replaceAll("[^A-Za-z0-9_\\-]", "_");
  }

  private String firstNonBlank(String primary, String fallback) {
    if (primary != null && !primary.isBlank()) {
      return primary.trim();
    }
    return fallback;
  }

  private void requireNonBlank(String value, String fieldName) {
    if (value == null || value.isBlank()) {
      throw new IllegalArgumentException(fieldName + " must not be blank");
    }
  }
}
