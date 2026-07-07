// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.time.Instant;
import java.time.LocalDateTime;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.time.format.DateTimeParseException;
import java.util.Locale;
import java.util.Objects;

/**
 * Parses AGV MQTT JSON status messages.
 */
public class AgvMqttStatusPayloadParser {

  private static final DateTimeFormatter LOCAL_DATE_TIME_FORMATTER
      = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");

  private final ObjectMapper objectMapper;

  public AgvMqttStatusPayloadParser(ObjectMapper objectMapper) {
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
  }

  public AgvMqttStatusMessage parse(String jsonPayload) {
    try {
      JsonNode root = objectMapper.readTree(jsonPayload);
      String agvId = firstText(root, "agv_id", "agvId", "vehicle_id", "vehicleId");
      String eventType = firstText(root, "event_type", "eventType", "type");
      if (agvId == null) {
        throw new IllegalArgumentException("agv_id must not be blank");
      }
      if (eventType == null) {
        throw new IllegalArgumentException("event_type must not be blank");
      }

      return new AgvMqttStatusMessage(
          firstText(root, "message_id", "messageId", "msg_id", "msgId"),
          agvId,
          eventType.trim().toUpperCase(Locale.ROOT),
          firstText(root, "mission_no", "missionNo", "order_name", "orderName"),
          firstText(root, "task_no", "taskNo"),
          firstText(root, "point_id", "pointId", "current_point", "currentPoint"),
          firstInt(root, "battery", "battery_level", "batteryLevel"),
          firstBoolean(root, "online", "is_online", "isOnline"),
          firstDouble(root, "x", "ros_x", "rosX"),
          firstDouble(root, "y", "ros_y", "rosY"),
          firstDouble(root, "yaw", "theta", "ros_yaw", "rosYaw"),
          firstText(root, "error_code", "errorCode", "reason_code", "reasonCode"),
          firstText(root, "error_msg", "errorMsg", "reason_msg", "reasonMsg"),
          firstLong(root, "seq", "sequence"),
          parseEventTime(firstText(root, "event_time", "eventTime", "timestamp", "time"))
      );
    }
    catch (IOException exc) {
      throw new IllegalArgumentException("Could not parse AGV MQTT payload", exc);
    }
  }

  private String firstText(JsonNode node, String... keys) {
    for (String key : keys) {
      JsonNode child = node.path(key);
      if (!child.isMissingNode() && !child.isNull()) {
        String value = child.asText();
        if (value != null && !value.isBlank()) {
          return value.trim();
        }
      }
    }
    return null;
  }

  private Integer firstInt(JsonNode node, String... keys) {
    for (String key : keys) {
      JsonNode child = node.path(key);
      if (child.isInt()) {
        return child.asInt();
      }
      if (child.isTextual()) {
        try {
          return Integer.parseInt(child.asText());
        }
        catch (NumberFormatException exc) {
          return null;
        }
      }
    }
    return null;
  }

  private Long firstLong(JsonNode node, String... keys) {
    for (String key : keys) {
      JsonNode child = node.path(key);
      if (child.isLong() || child.isInt()) {
        return child.asLong();
      }
      if (child.isTextual()) {
        try {
          return Long.parseLong(child.asText());
        }
        catch (NumberFormatException exc) {
          return null;
        }
      }
    }
    return null;
  }

  private Double firstDouble(JsonNode node, String... keys) {
    for (String key : keys) {
      JsonNode child = node.path(key);
      if (child.isNumber()) {
        return child.asDouble();
      }
      if (child.isTextual()) {
        try {
          return Double.parseDouble(child.asText());
        }
        catch (NumberFormatException exc) {
          return null;
        }
      }
    }
    String[] parentKeys = {"pose", "current_pose", "currentPose", "ros_pose", "rosPose"};
    for (String parentKey : parentKeys) {
      JsonNode parent = node.path(parentKey);
      if (parent.isObject()) {
        Double value = firstDouble(parent, keys);
        if (value != null) {
          return value;
        }
      }
    }
    return null;
  }

  private Boolean firstBoolean(JsonNode node, String... keys) {
    for (String key : keys) {
      JsonNode child = node.path(key);
      if (child.isBoolean()) {
        return child.asBoolean();
      }
      if (child.isTextual()) {
        return Boolean.parseBoolean(child.asText());
      }
    }
    return null;
  }

  private Instant parseEventTime(String value) {
    if (value == null) {
      return Instant.now();
    }
    try {
      return Instant.parse(value);
    }
    catch (DateTimeParseException exc) {
      LocalDateTime localDateTime = LocalDateTime.parse(value, LOCAL_DATE_TIME_FORMATTER);
      return localDateTime.toInstant(ZoneOffset.UTC);
    }
  }
}
