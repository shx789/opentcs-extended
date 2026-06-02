// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.Objects;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.opentcs.rcs.agvcommand.AgvCommandSender;
import org.opentcs.rcs.api.dto.Mission;

/**
 * Publishes legacy AGV robot_control commands over MQTT.
 */
public class MqttAgvRobotControlPublisher
    implements AgvCommandPublisher, AgvCommandSender {

  private final String brokerUri;
  private final String clientId;
  private final String topic;
  private final int qos;
  private final String username;
  private final String password;
  private final Map<String, Integer> pointIdsByName;
  private final double runSpeed;
  private final ObjectMapper objectMapper;

  public MqttAgvRobotControlPublisher(
      String brokerUri,
      String clientId,
      String topic,
      int qos,
      String username,
      String password,
      Map<String, Integer> pointIdsByName,
      double runSpeed,
      ObjectMapper objectMapper
  ) {
    this.brokerUri = requireNonBlank(brokerUri, "brokerUri");
    this.clientId = requireNonBlank(clientId, "clientId");
    this.topic = requireNonBlank(topic, "topic");
    if (qos < 0 || qos > 2) {
      throw new IllegalArgumentException("qos must be between 0 and 2");
    }
    this.qos = qos;
    this.username = normalizeNullable(username);
    this.password = normalizeNullable(password);
    this.pointIdsByName = Map.copyOf(Objects.requireNonNull(pointIdsByName, "pointIdsByName"));
    if (pointIdsByName.isEmpty()) {
      throw new IllegalArgumentException("pointIdsByName must not be empty when AGV command publishing is enabled");
    }
    if (runSpeed <= 0.0) {
      throw new IllegalArgumentException("runSpeed must be positive");
    }
    this.runSpeed = runSpeed;
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
  }

  @Override
  public void publishMissionStart(Mission mission) {
    send(buildMissionStartPayloadJson(mission));
  }

  @Override
  public void send(String payloadJson) {
    publish(payloadJson);
  }

  public String buildMissionStartPayloadJson(Mission mission) {
    return toJson(buildMissionStartPayload(mission));
  }

  Map<String, Object> buildMissionStartPayload(Mission mission) {
    Objects.requireNonNull(mission, "mission");
    Integer pointId = pointIdsByName.get(mission.toPoint());
    if (pointId == null) {
      throw new IllegalArgumentException("No AGV point id mapping for to_point: " + mission.toPoint());
    }
    return Map.of(
        "cmd_type", "interest_point_control",
        "cmd", "start",
        "id", pointId,
        "run_speed", runSpeed,
        "path_stop_time", 0,
        "path_mode", 0,
        "circulates", 1,
        "time", 0
    );
  }

  private void publish(String payloadJson) {
    MqttClient client = null;
    try {
      client = new MqttClient(brokerUri, clientId, new MemoryPersistence());
      client.connect(connectOptions());
      MqttMessage message = new MqttMessage(payloadJson.getBytes(StandardCharsets.UTF_8));
      message.setQos(qos);
      message.setRetained(false);
      client.publish(topic, message);
    }
    catch (MqttException exc) {
      throw new IllegalStateException("Could not publish AGV robot_control command", exc);
    }
    finally {
      if (client != null) {
        try {
          if (client.isConnected()) {
            client.disconnect();
          }
          client.close();
        }
        catch (MqttException ignored) {
          // Best-effort cleanup only.
        }
      }
    }
  }

  private MqttConnectOptions connectOptions() {
    MqttConnectOptions options = new MqttConnectOptions();
    options.setCleanSession(true);
    if (username != null) {
      options.setUserName(username);
    }
    if (password != null) {
      options.setPassword(password.toCharArray());
    }
    return options;
  }

  private String toJson(Map<String, Object> payload) {
    try {
      return objectMapper.writeValueAsString(payload);
    }
    catch (JsonProcessingException exc) {
      throw new IllegalStateException("Could not serialize AGV robot_control command", exc);
    }
  }

  private static String requireNonBlank(String value, String fieldName) {
    if (value == null || value.isBlank()) {
      throw new IllegalArgumentException(fieldName + " must not be blank");
    }
    return value.trim();
  }

  private static String normalizeNullable(String value) {
    if (value == null || value.isBlank()) {
      return null;
    }
    return value.trim();
  }
}
