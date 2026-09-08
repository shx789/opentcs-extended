// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.util.Comparator;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import org.opentcs.rcs.api.dto.CancelMissionResp;
import org.opentcs.rcs.api.dto.CreateMissionReq;
import org.opentcs.rcs.api.dto.CreateMissionResp;
import org.opentcs.rcs.api.dto.Mission;
import org.opentcs.rcs.api.dto.MissionSummaryResp;
import org.opentcs.rcs.api.dto.QueryMissionResp;
import org.opentcs.rcs.bridge.agv.AgvCommandPublisher;
import org.opentcs.rcs.bridge.opentcs.OpenTcsOrderClient;
import org.opentcs.rcs.bridge.opentcs.OpenTcsPayloadMapper;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsTransportOrderReq;
import org.opentcs.rcs.core.idem.IdempotencyResult;
import org.opentcs.rcs.core.idem.IdempotencyService;
import org.opentcs.rcs.core.mission.MissionCallbackTarget;
import org.opentcs.rcs.core.mission.MissionStore;
import org.opentcs.rcs.http.RequestContext;

/**
 * I-09 mission creation service with idempotency and openTCS order creation.
 */
public class WcsMissionService {

  private static final String BIZ_TYPE = "MISSION_CREATE";
  private static final String STATUS_RECEIVED = "RECEIVED";
  private static final String STATUS_DONE = "DONE";
  private static final String STATUS_FAILED = "FAILED";
  private static final String STATUS_CANCELED = "CANCELED";
  private static final Set<String> TERMINAL_STATUSES = Set.of(
      STATUS_DONE,
      STATUS_FAILED,
      STATUS_CANCELED
  );

  private final IdempotencyService idempotencyService;
  private final OpenTcsPayloadMapper payloadMapper;
  private final OpenTcsOrderClient openTcsOrderClient;
  private final ObjectMapper objectMapper;
  private final MissionStore missionStore;
  private final AgvCommandPublisher agvCommandPublisher;

  public WcsMissionService(
      IdempotencyService idempotencyService,
      OpenTcsPayloadMapper payloadMapper,
      OpenTcsOrderClient openTcsOrderClient,
      ObjectMapper objectMapper,
      MissionStore missionStore,
      AgvCommandPublisher agvCommandPublisher
  ) {
    this.idempotencyService = Objects.requireNonNull(idempotencyService, "idempotencyService");
    this.payloadMapper = Objects.requireNonNull(payloadMapper, "payloadMapper");
    this.openTcsOrderClient = Objects.requireNonNull(openTcsOrderClient, "openTcsOrderClient");
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
    this.missionStore = Objects.requireNonNull(missionStore, "missionStore");
    this.agvCommandPublisher = Objects.requireNonNull(agvCommandPublisher, "agvCommandPublisher");
  }

  public CreateMissionResp createMission(CreateMissionReq request, RequestContext requestContext) {
    validateRequest(request);
    Objects.requireNonNull(requestContext, "requestContext");
    IdempotencyResult idemResult = idempotencyService.check(BIZ_TYPE, request.missionNo(), request);
    if (idemResult.hit()) {
      CreateMissionResp cached = idemResult.cachedResponse(objectMapper, CreateMissionResp.class);
      return new CreateMissionResp(
          cached.missionNo(),
          cached.taskNo(),
          cached.rcsStatus(),
          true
      );
    }

    Mission mission = new Mission(
        request.missionNo(),
        request.taskNo(),
        request.fromPoint(),
        request.toPoint(),
        request.palletNo(),
        request.priority() == null ? 50 : request.priority(),
        request.callbackUrl(),
        request.missionType(),
        request.fromOperation(),
        request.toOperation()
    );
    OpenTcsTransportOrderReq payload = payloadMapper.toTransportOrderReq(mission);
    MissionCallbackTarget callbackTarget = new MissionCallbackTarget(
        mission.missionNo(),
        mission.taskNo(),
        mission.callbackUrl(),
        requestContext.traceId(),
        requestContext.requestId(),
        mission.fromPoint(),
        mission.toPoint(),
        mission.palletNo(),
        mission.priority()
    );

    openTcsOrderClient.createTransportOrder(mission.missionNo(), payload);
    missionStore.save(callbackTarget);
    try {
      agvCommandPublisher.publishMissionStart(mission);
    }
    catch (RuntimeException exc) {
      missionStore.updateStatus(mission.missionNo(), STATUS_FAILED);
      throw exc;
    }

    CreateMissionResp response = new CreateMissionResp(
        mission.missionNo(),
        mission.taskNo(),
        STATUS_RECEIVED,
        false
    );
    idempotencyService.storeSuccess(BIZ_TYPE, request.missionNo(), request, response);
    return response;
  }


  public List<MissionSummaryResp> listMissions() {
    return missionStore.findAll().stream()
        .sorted(Comparator.comparing(MissionCallbackTarget::missionNo))
        .map(
            target -> new MissionSummaryResp(
                target.missionNo(),
                target.taskNo(),
                target.rcsStatus(),
                target.fromPoint(),
                target.toPoint(),
                target.palletNo(),
                target.priority()
            )
        )
        .toList();
  }

  public QueryMissionResp queryMission(String missionNo) {
    requireNonBlank(missionNo, "missionNo");
    MissionCallbackTarget target = missionStore.findByMissionNo(missionNo)
        .orElseThrow(() -> new ResourceNotFoundException("missionNo not found: " + missionNo));
    return new QueryMissionResp(target.missionNo(), target.taskNo(), target.rcsStatus());
  }

  public CancelMissionResp cancelMission(String missionNo) {
    requireNonBlank(missionNo, "missionNo");
    MissionCallbackTarget target = missionStore.findByMissionNo(missionNo)
        .orElseThrow(() -> new ResourceNotFoundException("missionNo not found: " + missionNo));

    if (STATUS_CANCELED.equals(target.rcsStatus())) {
      return new CancelMissionResp(target.missionNo(), target.taskNo(), target.rcsStatus(), true);
    }
    if (TERMINAL_STATUSES.contains(target.rcsStatus())) {
      throw new TaskStateConflictException(
          "Mission already in terminal status: " + target.rcsStatus()
      );
    }

    openTcsOrderClient.cancelTransportOrder(missionNo);
    missionStore.updateStatus(missionNo, STATUS_CANCELED);
    return new CancelMissionResp(target.missionNo(), target.taskNo(), STATUS_CANCELED, false);
  }

  private void validateRequest(CreateMissionReq request) {
    Objects.requireNonNull(request, "request");
    requireNonBlank(request.missionNo(), "missionNo");
    requireNonBlank(request.taskNo(), "taskNo");
    requireNonBlank(request.fromPoint(), "fromPoint");
    requireNonBlank(request.toPoint(), "toPoint");
    requireNonBlank(request.palletNo(), "palletNo");
    requireNonBlank(request.callbackUrl(), "callbackUrl");
  }

  private void requireNonBlank(String value, String fieldName) {
    if (value == null || value.isBlank()) {
      throw new IllegalArgumentException(fieldName + " must not be blank");
    }
  }
}
