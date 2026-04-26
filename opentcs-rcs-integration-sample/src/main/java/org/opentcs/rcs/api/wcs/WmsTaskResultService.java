// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

import java.time.Instant;
import java.util.Objects;
import org.opentcs.rcs.api.dto.WmsTaskResultCallbackReq;
import org.opentcs.rcs.callback.CallbackOutboxService;
import org.opentcs.rcs.core.task.WcsTaskRecord;
import org.opentcs.rcs.core.task.WcsTaskStatus;
import org.opentcs.rcs.core.task.WcsTaskType;
import org.opentcs.rcs.http.RequestContext;

/**
 * Enqueues WCS -> WMS task result callbacks.
 */
public class WmsTaskResultService {

  private static final String INBOUND_RESULT_PATH = "/api/v1/wms/inbound-results";
  private static final String OUTBOUND_RESULT_PATH = "/api/v1/wms/outbound-results";

  private final CallbackOutboxService callbackOutboxService;
  private final String wmsBaseUrl;

  public WmsTaskResultService(
      CallbackOutboxService callbackOutboxService,
      String wmsBaseUrl
  ) {
    this.callbackOutboxService = Objects.requireNonNull(callbackOutboxService, "callbackOutboxService");
    this.wmsBaseUrl = normalizeBaseUrl(wmsBaseUrl);
  }

  public void enqueueResultIfConfigured(
      WcsTaskRecord task,
      WcsTaskStatus resultStatus,
      RequestContext requestContext,
      String reasonCode,
      String reasonMsg,
      String eventTime
  ) {
    Objects.requireNonNull(task, "task");
    Objects.requireNonNull(resultStatus, "resultStatus");
    Objects.requireNonNull(requestContext, "requestContext");
    if (wmsBaseUrl == null || !isResultStatus(resultStatus)) {
      return;
    }

    WmsTaskResultCallbackReq payload = new WmsTaskResultCallbackReq(
        task.bizTaskNo(),
        task.taskType().name(),
        resultStatus.name(),
        task.missionNo(),
        task.taskNo(),
        task.plcJobNo(),
        resultStatus.name(),
        normalizeOptional(reasonCode),
        normalizeOptional(reasonMsg),
        requestContext.traceId(),
        requestContext.requestId(),
        eventTime == null || eventTime.isBlank() ? Instant.now().toString() : eventTime.trim()
    );

    callbackOutboxService.enqueue(
        task.missionNo(),
        resolveResultCallbackUrl(task.taskType()),
        payload,
        task.bizTaskNo() + "|" + resultStatus.name()
    );
  }

  private boolean isResultStatus(WcsTaskStatus status) {
    return status == WcsTaskStatus.DONE
        || status == WcsTaskStatus.FAILED
        || status == WcsTaskStatus.CANCELED;
  }

  private String resolveResultCallbackUrl(WcsTaskType taskType) {
    String path = taskType == WcsTaskType.INBOUND ? INBOUND_RESULT_PATH : OUTBOUND_RESULT_PATH;
    return wmsBaseUrl + path;
  }

  private static String normalizeBaseUrl(String rawBaseUrl) {
    if (rawBaseUrl == null || rawBaseUrl.isBlank()) {
      return null;
    }
    String value = rawBaseUrl.trim();
    return value.endsWith("/") ? value.substring(0, value.length() - 1) : value;
  }

  private static String normalizeOptional(String value) {
    if (value == null || value.isBlank()) {
      return null;
    }
    return value.trim();
  }
}
