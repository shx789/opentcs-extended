// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv;

import java.nio.charset.StandardCharsets;
import java.util.Objects;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.opentcs.rcs.callback.CallbackRetryProcessor;
import org.opentcs.rcs.http.RequestContext;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Subscribes to AGV MQTT feedback topics.
 */
public class AgvMqttStatusSubscriber {

  private static final Logger LOG = LoggerFactory.getLogger(AgvMqttStatusSubscriber.class);

  private final String brokerUri;
  private final String clientId;
  private final String topicFilter;
  private final int qos;
  private final String username;
  private final String password;
  private final AgvMqttStatusPayloadParser payloadParser;
  private final AgvMqttStatusEventConsumer eventConsumer;
  private final CallbackRetryProcessor callbackRetryProcessor;
  private final int callbackDispatchBatchSize;

  private MqttClient client;

  public AgvMqttStatusSubscriber(
      String brokerUri,
      String clientId,
      String topicFilter,
      int qos,
      String username,
      String password,
      AgvMqttStatusPayloadParser payloadParser,
      AgvMqttStatusEventConsumer eventConsumer,
      CallbackRetryProcessor callbackRetryProcessor,
      int callbackDispatchBatchSize
  ) {
    this.brokerUri = requireNonBlank(brokerUri, "brokerUri");
    this.clientId = requireNonBlank(clientId, "clientId");
    this.topicFilter = requireNonBlank(topicFilter, "topicFilter");
    if (qos < 0 || qos > 2) {
      throw new IllegalArgumentException("qos must be between 0 and 2");
    }
    this.qos = qos;
    this.username = normalizeNullable(username);
    this.password = normalizeNullable(password);
    this.payloadParser = Objects.requireNonNull(payloadParser, "payloadParser");
    this.eventConsumer = Objects.requireNonNull(eventConsumer, "eventConsumer");
    this.callbackRetryProcessor = Objects.requireNonNull(callbackRetryProcessor, "callbackRetryProcessor");
    if (callbackDispatchBatchSize < 1) {
      throw new IllegalArgumentException("callbackDispatchBatchSize must be greater than 0");
    }
    this.callbackDispatchBatchSize = callbackDispatchBatchSize;
  }

  public synchronized void start() {
    if (client != null && client.isConnected()) {
      return;
    }
    try {
      client = new MqttClient(brokerUri, clientId, new MemoryPersistence());
      client.setCallback(new StatusCallback());
      client.connect(connectOptions());
      client.subscribe(topicFilter, qos);
      LOG.info("Subscribed AGV MQTT topic: broker={} topic={} qos={}", brokerUri, topicFilter, qos);
    }
    catch (MqttException exc) {
      throw new IllegalStateException("Could not start AGV MQTT subscriber", exc);
    }
  }

  public synchronized void stop() {
    if (client == null) {
      return;
    }
    try {
      if (client.isConnected()) {
        client.unsubscribe(topicFilter);
        client.disconnect();
      }
      client.close();
    }
    catch (MqttException exc) {
      LOG.warn("Could not stop AGV MQTT subscriber: {}", exc.getMessage());
    }
    finally {
      client = null;
    }
  }

  private MqttConnectOptions connectOptions() {
    MqttConnectOptions options = new MqttConnectOptions();
    options.setAutomaticReconnect(true);
    options.setCleanSession(true);
    if (username != null) {
      options.setUserName(username);
    }
    if (password != null) {
      options.setPassword(password.toCharArray());
    }
    return options;
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

  private class StatusCallback
      implements MqttCallback {

    @Override
    public void connectionLost(Throwable cause) {
      LOG.warn("AGV MQTT connection lost: {}", cause == null ? "<unknown>" : cause.getMessage());
    }

    @Override
    public void messageArrived(String topic, MqttMessage mqttMessage) {
      String payload = new String(mqttMessage.getPayload(), StandardCharsets.UTF_8);
      try {
        AgvMqttStatusMessage message = payloadParser.parse(payload);
        eventConsumer.consume(message, RequestContext.generated());
        callbackRetryProcessor.processDue(callbackDispatchBatchSize);
      }
      catch (RuntimeException exc) {
        LOG.warn("Could not process AGV MQTT message from topic {}: {}", topic, exc.getMessage());
      }
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token) {
    }
  }
}
