// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.util.Locale;
import java.util.Objects;
import java.util.Optional;
import org.opentcs.rcs.api.dto.AgvEventCallbackReq;
import org.opentcs.rcs.api.dto.WcsEventType;
import org.opentcs.rcs.http.RequestContext;

/**
 * Projects openTCS order state events to WCS callback events.
 */
public class OpenTcsEventProjector {

  private static final DateTimeFormatter EVENT_TIME_FORMATTER
      = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss").withZone(ZoneOffset.UTC);

  public Optional<AgvEventCallbackReq> toWcsEvent(
      OpenTcsTransportOrderEvent event,
      RequestContext requestContext
  ) {
    Objects.requireNonNull(event, "event");
    Objects.requireNonNull(requestContext, "requestContext");
    Optional<WcsEventType> mappedType = mapEventType(event);
    if (mappedType.isEmpty()) {
      return Optional.empty();
    }

    String reasonCode = null;
    String reasonMsg = null;
    if (mappedType.get() == WcsEventType.FAILED) {
      reasonMsg = event.failureReason() == null ? "openTCS order failed" : event.failureReason();
      reasonCode = failureReasonCode(reasonMsg);
    }

    return Optional.of(
        new AgvEventCallbackReq(
            event.missionNo(),
            event.taskNo(),
            mappedType.get().name(),
            event.currentPointId(),
            event.processingVehicle(),
            null,
            reasonCode,
            reasonMsg,
            requestContext.traceId(),
            requestContext.requestId(),
            EVENT_TIME_FORMATTER.format(event.eventTime())
        )
    );
  }

  private String failureReasonCode(String reason) {
    String normalized = reason == null ? "" : reason.toLowerCase(Locale.ROOT);
    if (normalized.contains("localization") || normalized.contains("amcl")) {
      return "LOCALIZATION_NOT_READY";
    }
    if (normalized.contains("magnetic") && normalized.contains("forward")) {
      return "MAGNETIC_FORWARD_FAILURE";
    }
    if (normalized.contains("magnetic") && normalized.contains("backward")) {
      return "MAGNETIC_BACKWARD_FAILURE";
    }
    if (normalized.contains("lift") && normalized.contains("up")) {
      return "LIFT_UP_FAILURE";
    }
    if (normalized.contains("lift") && normalized.contains("down")) {
      return "LIFT_DOWN_FAILURE";
    }
    if (normalized.contains("post-drop") || normalized.contains("post drop")) {
      return "POST_DROP_NAVIGATION_FAILURE";
    }
    if (normalized.contains("navigation") && normalized.contains("timed out")) {
      return "NAVIGATION_TIMEOUT";
    }
    if (normalized.contains("timeout") || normalized.contains("timed out")) {
      return "COMMAND_TIMEOUT";
    }
    if (normalized.contains("mqtt") && normalized.contains("connect")) {
      return "MQTT_CONNECTION_FAILURE";
    }
    if (normalized.contains("navigation") || normalized.contains("destination")) {
      return "NAVIGATION_FAILURE";
    }
    if (normalized.contains("pick")) {
      return "PICK_FAILURE";
    }
    if (normalized.contains("drop")) {
      return "DROP_FAILURE";
    }
    return "RCS_FAILED";
  }

  private Optional<WcsEventType> mapEventType(OpenTcsTransportOrderEvent event) {
    return switch (event.transportOrderState()) {
      case "BEING_PROCESSED" -> mapInProgressEvent(event.currentDriveOrderIndex());
      case "FINISHED" -> Optional.of(WcsEventType.DROPPED);
      case "FAILED" -> Optional.of(WcsEventType.FAILED);
      default -> Optional.empty();
    };
  }

  private Optional<WcsEventType> mapInProgressEvent(Integer currentDriveOrderIndex) {
    if (currentDriveOrderIndex == null) {
      return Optional.empty();
    }
    return switch (currentDriveOrderIndex) {
      case 0 -> Optional.of(WcsEventType.ARRIVED_FROM);
      case 1 -> Optional.of(WcsEventType.ARRIVED_TO);
      default -> Optional.empty();
    };
  }
}
