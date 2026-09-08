// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.time.Instant;
import java.util.Objects;

/**
 * Parses raw openTCS SSE transport-order payload JSON.
 */
public class OpenTcsSsePayloadParser {

  private final ObjectMapper objectMapper;

  public OpenTcsSsePayloadParser(ObjectMapper objectMapper) {
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
  }

  public OpenTcsTransportOrderEvent parseTransportOrderEvent(String jsonPayload) {
    try {
      JsonNode root = objectMapper.readTree(jsonPayload);
      JsonNode current = root.path("currentObjectState");
      JsonNode previous = root.path("previousObjectState");
      JsonNode effective = current.isMissingNode() || current.isNull() ? previous : current;

      String missionNo = textValue(effective, "name");
      String taskNo = textValue(effective.path("properties"), "task_no");
      String state = textValue(effective, "state");
      Integer currentDriveOrderIndex = effective.path("currentDriveOrderIndex").isNumber()
          ? effective.path("currentDriveOrderIndex").asInt()
          : null;
      Integer currentRouteStepIndex = effective.path("currentRouteStepIndex").isNumber()
          ? effective.path("currentRouteStepIndex").asInt()
          : null;
      String currentPointId = extractCurrentPointId(
          effective,
          state,
          currentDriveOrderIndex,
          currentRouteStepIndex
      );
      String processingVehicle = textValue(effective, "processingVehicle");
      String failureReason = textValue(effective.path("history"), "lastEntry");

      Instant eventTime = parseTimestamp(root.path("eventTime"));
      if (eventTime == null) {
        eventTime = parseTimestamp(root.path("timeStamp"));
      }
      if (eventTime == null) {
        eventTime = Instant.now();
      }

      return new OpenTcsTransportOrderEvent(
          missionNo,
          taskNo,
          state,
          currentDriveOrderIndex,
          currentPointId,
          processingVehicle,
          failureReason,
          eventTime
      );
    }
    catch (IOException exc) {
      throw new IllegalArgumentException("Could not parse openTCS SSE payload", exc);
    }
  }

  private String textValue(JsonNode node, String key) {
    JsonNode child = node.path(key);
    if (child.isMissingNode() || child.isNull()) {
      return null;
    }
    String text = child.asText();
    return text.isBlank() ? null : text;
  }

  private Instant parseTimestamp(JsonNode timestampNode) {
    if (timestampNode == null || timestampNode.isMissingNode() || timestampNode.isNull()) {
      return null;
    }
    String ts = timestampNode.asText();
    if (ts == null || ts.isBlank()) {
      return null;
    }
    try {
      return Instant.parse(ts);
    }
    catch (RuntimeException exc) {
      return null;
    }
  }

  private String extractCurrentPointId(
      JsonNode effective,
      String orderState,
      Integer currentDriveOrderIndex,
      Integer currentRouteStepIndex
  ) {
    JsonNode driveOrders = effective.path("driveOrders");
    if (!driveOrders.isArray() || driveOrders.isEmpty()) {
      return null;
    }

    // For finished orders, report the final destination point.
    if ("FINISHED".equals(orderState)) {
      return destinationByDriveOrderIndex(driveOrders, driveOrders.size() - 1);
    }

    Integer driveOrderIndex = normalizeIndex(currentDriveOrderIndex, driveOrders.size());
    if (driveOrderIndex == null) {
      driveOrderIndex = findLastFinishedDriveOrderIndex(driveOrders);
      if (driveOrderIndex == null) {
        driveOrderIndex = 0;
      }
    }

    JsonNode currentDriveOrder = driveOrders.get(driveOrderIndex);
    String byRouteStep = currentPointByRouteStep(currentDriveOrder, currentRouteStepIndex);
    if (byRouteStep != null) {
      return byRouteStep;
    }
    return destinationByDriveOrderIndex(driveOrders, driveOrderIndex);
  }

  private String destinationByDriveOrderIndex(JsonNode driveOrders, Integer index) {
    if (index == null || index < 0 || index >= driveOrders.size()) {
      return null;
    }
    JsonNode driveOrder = driveOrders.get(index);
    return textValue(driveOrder.path("destination"), "destination");
  }

  private Integer normalizeIndex(Integer index, int size) {
    if (index == null || index < 0 || index >= size) {
      return null;
    }
    return index;
  }

  private Integer findLastFinishedDriveOrderIndex(JsonNode driveOrders) {
    Integer result = null;
    for (int index = 0; index < driveOrders.size(); index++) {
      if ("FINISHED".equals(textValue(driveOrders.get(index), "state"))) {
        result = index;
      }
    }
    return result;
  }

  private String currentPointByRouteStep(JsonNode driveOrder, Integer currentRouteStepIndex) {
    JsonNode steps = driveOrder.path("route").path("steps");
    if (!steps.isArray() || steps.isEmpty()) {
      return null;
    }
    if (currentRouteStepIndex == null || currentRouteStepIndex < 0) {
      return textValue(steps.get(0), "sourcePoint");
    }
    if (currentRouteStepIndex >= steps.size()) {
      return textValue(steps.get(steps.size() - 1), "destinationPoint");
    }
    JsonNode currentStep = steps.get(currentRouteStepIndex);
    String source = textValue(currentStep, "sourcePoint");
    return source != null ? source : textValue(currentStep, "destinationPoint");
  }
}
