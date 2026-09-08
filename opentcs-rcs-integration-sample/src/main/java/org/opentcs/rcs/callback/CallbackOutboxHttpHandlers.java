// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

import com.fasterxml.jackson.annotation.JsonProperty;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import io.javalin.http.Handler;
import java.time.Instant;
import java.util.Objects;
import org.opentcs.rcs.api.dto.ApiResponse;

/**
 * HTTP handlers for inspecting callback outbox records.
 */
public final class CallbackOutboxHttpHandlers {

  private CallbackOutboxHttpHandlers() {
  }

  public static Handler listCallbacksHandler(
      CallbackOutboxStore store,
      ObjectMapper objectMapper
  ) {
    Objects.requireNonNull(store, "store");
    Objects.requireNonNull(objectMapper, "objectMapper");
    return ctx -> {
      String missionNo = normalize(ctx.queryParam("mission_no"));
      var records = store.findAll().stream()
          .filter(entry -> missionNo == null || missionNo.equals(entry.missionNo()))
          .map(entry -> toResponse(entry, objectMapper))
          .toList();
      ctx.status(200).json(ApiResponse.success(records));
    };
  }

  private static CallbackOutboxRecordResp toResponse(
      CallbackOutboxEntry entry,
      ObjectMapper objectMapper
  ) {
    return new CallbackOutboxRecordResp(
        entry.missionNo(),
        entry.callbackUrl(),
        entry.status(),
        entry.retryCount(),
        entry.lastError(),
        entry.nextRetryAt(),
        entry.idemKey(),
        readPayload(entry.payloadJson(), objectMapper)
    );
  }

  private static JsonNode readPayload(String payloadJson, ObjectMapper objectMapper) {
    try {
      return objectMapper.readTree(payloadJson);
    }
    catch (Exception exc) {
      return objectMapper.getNodeFactory().textNode(payloadJson);
    }
  }

  private static String normalize(String value) {
    if (value == null || value.isBlank()) {
      return null;
    }
    return value.trim();
  }

  public record CallbackOutboxRecordResp(
      @JsonProperty("mission_no")
      String missionNo,
      @JsonProperty("callback_url")
      String callbackUrl,
      @JsonProperty("status")
      String status,
      @JsonProperty("retry_count")
      int retryCount,
      @JsonProperty("last_error")
      String lastError,
      @JsonProperty("next_retry_at")
      Instant nextRetryAt,
      @JsonProperty("idem_key")
      String idemKey,
      @JsonProperty("payload")
      JsonNode payload
  ) {
  }
}
