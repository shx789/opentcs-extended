// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.mqttvehicle;

import static java.util.Objects.requireNonNull;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.google.inject.assistedinject.Assisted;
import jakarta.annotation.Nonnull;
import jakarta.inject.Inject;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.opentcs.customizations.kernel.KernelExecutor;
import org.opentcs.data.TCSObjectReference;
import org.opentcs.data.model.Point;
import org.opentcs.data.model.Vehicle;
import org.opentcs.data.order.TransportOrder;
import org.opentcs.drivers.vehicle.BasicVehicleCommAdapter;
import org.opentcs.drivers.vehicle.LoadHandlingDevice;
import org.opentcs.drivers.vehicle.MovementCommand;
import org.opentcs.drivers.vehicle.VehicleCommAdapterMessage;
import org.opentcs.drivers.vehicle.VehicleProcessModel;
import org.opentcs.util.ExplainedBoolean;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * A communication adapter that controls an AGV through MQTT.
 */
public class MqttCommAdapter
    extends
      BasicVehicleCommAdapter {

  /**
   * The name of the load handling device set by this adapter.
   */
  public static final String LHD_NAME = "default";
  /**
   * Vehicle property: MQTT broker URI.
   */
  public static final String PROP_BROKER_URI = "mqtt:brokerUri";
  /**
   * Vehicle property: MQTT command topic.
   */
  public static final String PROP_COMMAND_TOPIC = "mqtt:commandTopic";
  /**
   * Vehicle property: MQTT feedback topic.
   */
  public static final String PROP_FEEDBACK_TOPIC = "mqtt:feedbackTopic";
  /**
   * Vehicle property: MQTT base status topic.
   */
  public static final String PROP_BASE_STATUS_TOPIC = "mqtt:baseStatusTopic";
  /**
   * Vehicle property: MQTT point id map.
   */
  public static final String PROP_POINT_ID_MAP = "mqtt:pointIdMap";
  /**
   * Vehicle property: Initial logical openTCS position.
   */
  public static final String PROP_INITIAL_POSITION = "mqtt:initialPosition";
  /**
   * Vehicle property: AGV id used for optional feedback filtering.
   */
  public static final String PROP_AGV_ID = "mqtt:agvId";
  /**
   * Vehicle property: Delay in milliseconds to wait after a lift operation's
   * sensor state has settled before completing the command.
   */
  public static final String PROP_LIFT_SETTLE_TIME = "mqtt:liftSettleTimeMillis";
  /**
   * This class's logger.
   */
  private static final Logger LOG = LoggerFactory.getLogger(MqttCommAdapter.class);
  private static final Pattern TRAILING_NUMBER_PATTERN = Pattern.compile("(\\d+)$");
  private static final int DEFAULT_COMMAND_QUEUE_CAPACITY = 1;
  private static final String DEFAULT_RECHARGE_OPERATION = "CHARGE";
  private static final String OPERATION_PICK = "PICK";
  private static final String OPERATION_LOAD = "LOAD";
  private static final String OPERATION_LIFT_UP = "LIFT_UP";
  private static final String OPERATION_DROP = "DROP";
  private static final String OPERATION_UNLOAD = "UNLOAD";
  private static final String OPERATION_LIFT_DOWN = "LIFT_DOWN";
  private static final String MESSAGE_SET_POSITION = "tcs:virtualVehicle:setPosition";
  private static final String MESSAGE_RESET_POSITION = "tcs:virtualVehicle:resetPosition";
  private static final String PARAM_POSITION = "position";
  /**
   * Navigation system mode reported by base_status "status" when the AGV is
   * in its error state. Other modes (idle, navigating, multi-task, ...) are
   * normal operating states and must not fail the current command.
   */
  private static final int NAV_STATUS_ERROR = 9;

  private final Vehicle vehicle;
  private final ObjectMapper objectMapper = new ObjectMapper();
  private final Map<String, Integer> pointIdsByName = new HashMap<>();
  private final Map<String, String> pointNamesByFeedbackKey = new HashMap<>();
  private boolean initialized;
  private MqttClient mqttClient;
  private MqttSettings settings;
  private String cachedFinalFeedbackOrderName;
  private String cachedFinalFeedbackPointName;
  private MovementCommand pendingLiftOperationCommand;
  private String pendingLiftOperation;
  private boolean liftCompletionScheduled;
  private boolean hasMagneticStatus;
  private boolean materialPresent;
  private boolean liftUp;
  private boolean liftDown;
  private boolean liftTaskSuccess;
  private boolean liftCommandPublished;
  private boolean hasLocalizationStatus;
  private boolean localizationReady;
  private double amclScore;
  private MovementCommand pendingMagneticTransitCommand;
  private int pendingMagneticTransitAction;
  private MovementCommand pendingSpecialPointNavigationCommand;
  private MovementCommand pendingPostDropNavigationCommand;
  private ScheduledFuture<?> commandTimeoutFuture;
  private ScheduledFuture<?> idleReturnFuture;
  private boolean idleReturnActive;
  private String idleReturnPointName;

  /**
   * Creates a new instance.
   *
   * @param vehicle The vehicle this adapter is associated with.
   * @param kernelExecutor The kernel's executor.
   */
  @Inject
  public MqttCommAdapter(
      @Assisted
      Vehicle vehicle,
      @KernelExecutor
      ScheduledExecutorService kernelExecutor
  ) {
    super(
        new VehicleProcessModel(vehicle),
        DEFAULT_COMMAND_QUEUE_CAPACITY,
        DEFAULT_RECHARGE_OPERATION,
        kernelExecutor
    );
    this.vehicle = requireNonNull(vehicle, "vehicle");
  }

  @Override
  public void initialize() {
    if (isInitialized()) {
      return;
    }
    super.initialize();
    settings = MqttSettings.from(vehicle.getProperties());
    rebuildPointMappings(settings.pointIdMap());

    TCSObjectReference<Point> currentPosition = vehicle.getCurrentPosition();
    if (currentPosition != null) {
      getProcessModel().setPosition(currentPosition.getName());
    }
    else if (!settings.initialPosition().isBlank()) {
      getProcessModel().setPosition(settings.initialPosition());
    }
    getProcessModel().setEnergyLevel(100);
    getProcessModel().setLoadHandlingDevices(
        java.util.List.of(new LoadHandlingDevice(LHD_NAME, false))
    );
    getProcessModel().setState(Vehicle.State.IDLE);
    initialized = true;
  }

  @Override
  public boolean isInitialized() {
    return initialized;
  }

  @Override
  public void terminate() {
    if (!isInitialized()) {
      return;
    }
    disconnectVehicle();
    super.terminate();
    initialized = false;
  }

  @Override
  public synchronized void sendCommand(MovementCommand cmd)
      throws IllegalArgumentException {
    requireNonNull(cmd, "cmd");
    if (idleReturnActive) {
      publishIdleReturnStop();
      idleReturnActive = false;
      idleReturnPointName = null;
      getProcessModel().setState(Vehicle.State.IDLE);
      LOG.info("{}: Interrupted idle return for a new transport command.", getName());
    }
    cancelIdleReturn();

    if (!isVehicleConnected()) {
      throw new IllegalArgumentException("MQTT client is not connected.");
    }
    if (isNavigationCommandCandidate(cmd) && hasLocalizationStatus && !localizationReady) {
      LOG.warn(
          "{}: Blocking command because AGV localization is not ready: location={} amcl={}",
          getName(),
          localizationReady,
          amclScore
      );
      throw new IllegalArgumentException("AGV localization is not ready for navigation.");
    }

    String destinationPoint = cmd.getStep().getDestinationPoint().getName();
    // A cached final-destination navigation feedback may complete a route
    // step locally, but it must never bypass a pending no-path PICK/DROP
    // operation. Those commands represent physical lift actions and must
    // always reach startLiftOperation(), which publishes magnetic_nav.
    if (hasCachedFinalFeedbackFor(cmd) && !isLiftOperation(cmd.getOperation())) {
      LOG.info(
          "{}: Completing cached final-feedback route step locally: order={} stepDestination={} "
              + "finalDestination={}",
          getName(),
          cmd.getTransportOrder().getName(),
          destinationPoint,
          cmd.getFinalDestination().getName()
      );
      getExecutor().execute(() -> finishMovementCommand(cmd, destinationPoint));
      return;
    }

    if (cmd.getStep().getPath() == null) {
      if (isLiftOperation(cmd.getOperation())) {
        pendingSpecialPointNavigationCommand = cmd;
        int pointId = resolvePointId(destinationPoint)
            .orElseThrow(
                () -> new IllegalArgumentException(
                    "No MQTT point id mapping for destination point: " + destinationPoint
                )
            );
        publishCommand(
            toJson(buildCommandPayload(cmd, destinationPoint, destinationPoint, pointId))
        );
        scheduleCommandTimeout(
            cmd, settings.navigationTimeoutSeconds(), "pickup/dropoff start navigation"
        );
        getProcessModel().setState(Vehicle.State.EXECUTING);
        LOG.info(
            "{}: Published mandatory navigation confirmation to point {} before {}",
            getName(), destinationPoint, cmd.getOperation()
        );
        return;
      }
      else {
        getExecutor().execute(() -> finishMovementCommand(cmd, destinationPoint));
      }
      return;
    }

    String mqttTargetPoint = cmd.getFinalDestination().getName();
    int pointId = resolvePointId(mqttTargetPoint)
        .orElseThrow(
            () -> new IllegalArgumentException(
                "No MQTT point id mapping for destination point: " + mqttTargetPoint
            )
        );

    String payloadJson = toJson(
        buildCommandPayload(cmd, destinationPoint, mqttTargetPoint, pointId)
    );
    publishCommand(payloadJson);
    scheduleCommandTimeout(cmd, settings.navigationTimeoutSeconds(), "navigation");
    getProcessModel().setState(Vehicle.State.EXECUTING);
    LOG.info(
        "{}: Published MQTT command to {}: stepDestination={} mqttTarget={} id={}",
        getName(),
        settings.commandTopic(),
        destinationPoint,
        mqttTargetPoint,
        pointId
    );
  }

  @Override
  public synchronized ExplainedBoolean canProcess(TransportOrder order) {
    requireNonNull(order, "order");
    return new ExplainedBoolean(true, "");
  }

  @Override
  public void onVehiclePaused(boolean paused) {
    if (paused) {
      publishStopForCurrentCommand();
      getProcessModel().setState(Vehicle.State.IDLE);
    }
  }

  @Override
  public void processMessage(
      @Nonnull
      VehicleCommAdapterMessage message
  ) {
    requireNonNull(message, "message");

    switch (message.getType()) {
      case MESSAGE_SET_POSITION -> {
        String position = message.getParameters().get(PARAM_POSITION);
        if (position != null && !position.isBlank()) {
          getProcessModel().setPosition(position.trim());
          getProcessModel().setState(Vehicle.State.IDLE);
          LOG.info("{}: Manually set openTCS position to {}", getName(), position.trim());
        }
      }
      case MESSAGE_RESET_POSITION -> {
        getProcessModel().setPosition(null);
        LOG.info("{}: Manually reset openTCS position.", getName());
      }
      default -> {
        // Other message types are intentionally ignored by this adapter.
      }
    }
  }

  @Override
  protected synchronized void connectVehicle() {
    settings = MqttSettings.from(vehicle.getProperties());
    rebuildPointMappings(settings.pointIdMap());

    try {
      if (mqttClient != null && mqttClient.isConnected()) {
        return;
      }

      mqttClient = new MqttClient(
          settings.brokerUri(),
          settings.clientId(getName()),
          new MemoryPersistence()
      );
      mqttClient.setCallback(new AdapterMqttCallback());
      mqttClient.connect(connectOptions());
      subscribeFeedbackTopics();
      getProcessModel().setCommAdapterConnected(true);
      getProcessModel().setState(Vehicle.State.IDLE);
      scheduleIdleReturn();
      LOG.info(
          "{}: Connected MQTT broker={} feedbackTopic={}",
          getName(),
          settings.brokerUri(),
          settings.feedbackTopic()
      );
    }
    catch (MqttException exc) {
      getProcessModel().setCommAdapterConnected(false);
      getProcessModel().setState(Vehicle.State.UNKNOWN);
      LOG.warn("{}: Could not connect MQTT broker: {}", getName(), exc.getMessage());
      getExecutor().schedule(this::retryConnectIfEnabled, 5, TimeUnit.SECONDS);
    }
  }

  @Override
  protected synchronized void disconnectVehicle() {
    cancelCommandTimeout();
    pendingLiftOperationCommand = null;
    pendingMagneticTransitCommand = null;
    pendingMagneticTransitAction = 0;
    pendingPostDropNavigationCommand = null;
    pendingSpecialPointNavigationCommand = null;
    pendingPostDropNavigationCommand = null;
    cancelIdleReturn();
    idleReturnActive = false;
    idleReturnPointName = null;
    if (mqttClient == null) {
      getProcessModel().setCommAdapterConnected(false);
      return;
    }

    try {
      if (mqttClient.isConnected()) {
        mqttClient.unsubscribe(settings.feedbackTopic());
        if (!settings.baseStatusTopic().equals(settings.feedbackTopic())) {
          mqttClient.unsubscribe(settings.baseStatusTopic());
        }
        mqttClient.disconnect();
      }
      mqttClient.close();
    }
    catch (MqttException exc) {
      LOG.warn("{}: Could not disconnect MQTT client: {}", getName(), exc.getMessage());
    }
    finally {
      mqttClient = null;
      getProcessModel().setCommAdapterConnected(false);
    }
  }

  @Override
  protected synchronized boolean isVehicleConnected() {
    return mqttClient != null && mqttClient.isConnected();
  }

  private void retryConnectIfEnabled() {
    if (isEnabled() && !isVehicleConnected()) {
      connectVehicle();
    }
  }

  private void subscribeFeedbackTopics()
      throws MqttException {
    mqttClient.subscribe(settings.feedbackTopic(), settings.qos());
    if (!settings.baseStatusTopic().equals(settings.feedbackTopic())) {
      mqttClient.subscribe(settings.baseStatusTopic(), settings.qos());
    }
  }

  private MqttConnectOptions connectOptions() {
    MqttConnectOptions options = new MqttConnectOptions();
    options.setAutomaticReconnect(true);
    options.setCleanSession(true);
    options.setConnectionTimeout(settings.connectionTimeoutSeconds());
    options.setKeepAliveInterval(settings.keepAliveSeconds());
    if (!settings.username().isBlank()) {
      options.setUserName(settings.username());
    }
    if (!settings.password().isBlank()) {
      options.setPassword(settings.password().toCharArray());
    }
    return options;
  }

  private Map<String, Object> buildCommandPayload(
      MovementCommand command,
      String stepDestinationPoint,
      String mqttTargetPoint,
      int pointId
  ) {
    Map<String, Object> payload = new LinkedHashMap<>();
    payload.put("cmd_type", "interest_point_control");
    payload.put("cmd", "start");
    payload.put("id", pointId);
    payload.put("run_speed", settings.runSpeed());
    payload.put("path_stop_time", 0);
    payload.put("path_mode", 2);
    payload.put("circulates", 0);
    payload.put("time", 0);
    payload.put("opentcs_vehicle", getName());
    payload.put("opentcs_order", command.getTransportOrder().getName());
    payload.put("opentcs_dest", mqttTargetPoint);
    payload.put("opentcs_step_dest", stepDestinationPoint);
    return payload;
  }

  Map<String, Object> buildLiftCommandPayload(String operation) {
    Map<String, Object> payload = new LinkedHashMap<>();
    payload.put("cmd_type", "magnetic_nav");
    payload.put("aim_id", 0);
    payload.put("aim_dir", 0);
    payload.put("aim_action", liftAction(operation));
    payload.put("opentcs_vehicle", getName());
    payload.put("opentcs_operation", operation);
    return payload;
  }

  private void publishCommand(String payloadJson) {
    if (mqttClient == null) {
      LOG.warn("{}: MQTT client is not connected; cannot publish command yet.", getName());
      return;
    }
    try {
      MqttMessage message = new MqttMessage(payloadJson.getBytes(StandardCharsets.UTF_8));
      message.setQos(settings.qos());
      message.setRetained(false);
      mqttClient.publish(settings.commandTopic(), message);
    }
    catch (MqttException exc) {
      throw new IllegalArgumentException("Could not publish MQTT command.", exc);
    }
  }

  private void publishStopForCurrentCommand() {
    MovementCommand currentCommand = getSentCommands().peek();
    if (currentCommand == null) {
      return;
    }
    publishStopForCommand(currentCommand);
  }

  private void publishLiftOperationCommand(MovementCommand command) {
    String operation = normalizedOperation(command.getOperation());
    pendingLiftOperationCommand = command;
    pendingLiftOperation = operation;
    liftCompletionScheduled = false;
    liftCommandPublished = true;
    liftTaskSuccess = false;
    publishCommand(toJson(buildLiftCommandPayload(operation)));
    getProcessModel().setState(Vehicle.State.EXECUTING);
    LOG.info(
        "{}: Published MQTT lift command to {}: operation={}",
        getName(),
        settings.commandTopic(),
        operation
    );
  }

  private String toJson(Map<String, Object> payload) {
    try {
      return objectMapper.writeValueAsString(payload);
    }
    catch (JsonProcessingException exc) {
      throw new IllegalArgumentException("Could not serialize MQTT command.", exc);
    }
  }

  synchronized void handleFeedback(String topic, String payloadJson) {
    try {
      FeedbackMessage feedback = FeedbackMessage.parse(objectMapper.readTree(payloadJson));
      if ("base_status".equalsIgnoreCase(feedback.cmdType())) {
        // base_status is reported periodically (roughly once per second).
        // Log it at DEBUG level to avoid flooding the kernel log; warnings
        // and error-mode failures derived from it are still logged normally.
        LOG.debug(
            "{}: Received MQTT base_status topic={} material={} up={} down={} energy={} "
                + "mainerror={} suberror={} robot_status={} navStatus={}",
            getName(),
            topic,
            feedback.material(),
            feedback.up(),
            feedback.down(),
            feedback.energyLevel(),
            feedback.mainError(),
            feedback.subError(),
            feedback.robotStatus(),
            feedback.navStatus(),
            feedback.amcl(),
            feedback.location()
        );
      }
      else {
        LOG.info(
            "{}: Received MQTT feedback topic={} cmdType={} type={} id={} status={} pointName={}",
            getName(),
            topic,
            feedback.cmdType(),
            feedback.type(),
            feedback.pointId(),
            feedback.status(),
            feedback.pointName()
        );
      }
      if (!feedback.isSupportedFeedback()) {
        LOG.debug(
            "{}: Ignoring unsupported MQTT message on {}: {}", getName(), topic, payloadJson
        );
        return;
      }
      String mappedPointName = pointNameForFeedback(feedback.type(), feedback.pointId());
      if (feedback.pointName() == null && mappedPointName != null) {
        feedback = new FeedbackMessage(
            feedback.cmdType(),
            feedback.agvId(),
            feedback.type(),
            feedback.pointId(),
            mappedPointName,
            feedback.status(),
            feedback.energyLevel(),
            feedback.material(),
            feedback.up(),
            feedback.down(),
            feedback.mainError(),
            feedback.subError(),
            feedback.robotStatus(),
            feedback.navStatus(),
            feedback.amcl(),
            feedback.location()
        );
        LOG.info(
            "{}: Mapped MQTT feedback type={} id={} to openTCS point={}",
            getName(),
            feedback.type(),
            feedback.pointId(),
            mappedPointName
        );
      }
      if (!feedback.matchesAgv(settings.agvId())) {
        return;
      }
      if (feedback.energyLevel() != null) {
        getProcessModel().setEnergyLevel(feedback.energyLevel());
      }
      if (feedback.cmdType() != null
          && feedback.cmdType().equals("base_status")
          && feedback.hasMagneticStatus()) {
        hasMagneticStatus = true;
        materialPresent = feedback.material();
        liftUp = feedback.up();
        liftDown = feedback.down();
      }
      if ("base_status".equalsIgnoreCase(feedback.cmdType()) && feedback.hasLocalization()) {
        hasLocalizationStatus = true;
        amclScore = feedback.amcl();
        localizationReady = feedback.location() && amclScore >= settings.minAmclScore();
      }
      if (pendingLiftOperationCommand != null
          && "task_feedback".equals(feedback.cmdType())
          && "magnetic_nav".equalsIgnoreCase(feedback.type())
          && feedback.isSuccess()) {
        liftTaskSuccess = true;
        LOG.info(
            "{}: AGV magnetic_nav operation {} reported success.",
            getName(),
            pendingLiftOperation
        );
      }
      if (pendingLiftOperationCommand != null
          && "task_feedback".equals(feedback.cmdType())
          && "magnetic_nav".equalsIgnoreCase(feedback.type())
          && feedback.isFailure()) {
        failPendingLiftOperation(feedback);
        return;
      }
      if (pendingMagneticTransitCommand != null
          && "task_feedback".equals(feedback.cmdType())
          && "magnetic_nav".equalsIgnoreCase(feedback.type())
          && feedback.isSuccess()) {
        MovementCommand transitCommand = pendingMagneticTransitCommand;
        int transitAction = pendingMagneticTransitAction;
        pendingMagneticTransitCommand = null;
        pendingMagneticTransitAction = 0;
        LOG.info(
            "{}: Magnetic transit action {} completed for operation {}.",
            getName(), transitAction, transitCommand.getOperation()
        );
        if (transitAction == 1) {
          startLiftOperation(transitCommand);
        }
        else if (isLiftDownOperation(transitCommand.getOperation())) {
          startPostDropNavigation(transitCommand);
        }
        else {
          finishMovementCommand(
              transitCommand, transitCommand.getStep().getDestinationPoint().getName()
          );
        }
        return;
      }
      if (pendingMagneticTransitCommand != null
          && "task_feedback".equals(feedback.cmdType())
          && "magnetic_nav".equalsIgnoreCase(feedback.type())
          && feedback.isFailure()) {
        failMagneticTransit(feedback);
        return;
      }
      // The robot mainerror/suberror fields are not a reliable error signal:
      // the AGV may report them while operating normally. Only the
      // navigation mode status=9 indicates a real error, so the raw codes
      // are logged at DEBUG with the periodic base_status frame and the
      // warning is reserved for the actual error mode below.
      if (feedback.isNavError()) {
        LOG.error(
            "{}: AGV base_status reports navigation error mode status={}: robot_status={} "
                + "mainerror={} suberror={}",
            getName(),
            feedback.navStatus(),
            feedback.robotStatus(),
            feedback.mainError(),
            feedback.subError()
        );
        failCurrentCommandOnNavError(feedback);
        return;
      }

      if (pendingLiftOperationCommand != null && liftCompletionConditionsSatisfied()) {
        completePendingLiftOperation();
        return;
      }

      if (feedback.isProgress()) {
        getProcessModel().setState(Vehicle.State.EXECUTING);
        return;
      }

      if (feedback.isSuccess()) {
        if (idleReturnActive) {
          completeIdleReturn(feedback);
          return;
        }
        if (pendingPostDropNavigationCommand != null) {
          completePostDropNavigation(feedback);
          return;
        }
        // A lift command is completed only by the magnetic sensors. A generic
        // task success must not cause the lift command to be sent again.
        if (pendingLiftOperationCommand != null) {
          LOG.debug(
              "{}: Ignoring generic success while waiting for {} sensor state.",
              getName(),
              pendingLiftOperation
          );
          return;
        }
        completeCurrentCommand(feedback);
      }
      else if (feedback.isFailure()) {
        if (idleReturnActive) {
          failIdleReturn(feedback);
          return;
        }
        if (pendingPostDropNavigationCommand != null) {
          failPostDropNavigation(feedback);
          return;
        }
        failCurrentCommand(feedback);
      }
      else {
        LOG.debug("{}: Ignoring MQTT feedback on {}: {}", getName(), topic, payloadJson);
      }
    }
    catch (JsonProcessingException exc) {
      LOG.warn("{}: Could not parse MQTT feedback: {}", getName(), exc.getMessage());
    }
    catch (IllegalArgumentException exc) {
      LOG.warn("{}: Could not process MQTT feedback: {}", getName(), exc.getMessage());
    }
  }

  private void completeCurrentCommand(FeedbackMessage feedback) {
    MovementCommand command = getSentCommands().peek();
    if (command == null) {
      if (feedback.pointName() != null) {
        getProcessModel().setPosition(feedback.pointName());
        getProcessModel().setState(Vehicle.State.IDLE);
        LOG.info(
            "{}: Updated openTCS position from MQTT feedback without current command: {}",
            getName(),
            feedback.pointName()
        );
      }
      else {
        LOG.debug("{}: Ignoring success feedback without a current command.", getName());
      }
      return;
    }

    String destinationPoint = command.getStep().getDestinationPoint().getName();
    LOG.info(
        "{}: Processing successful MQTT feedback for command: feedbackPoint={} commandPoint={} "
            + "operation={} order={}",
        getName(),
        feedback.pointName(),
        destinationPoint,
        command.getOperation(),
        command.getTransportOrder().getName()
    );
    if (!feedbackMatchesCommand(feedback, destinationPoint)) {
      String finalDestinationPoint = command.getFinalDestination().getName();
      if (feedbackMatchesCommand(feedback, finalDestinationPoint)) {
        cacheFinalFeedback(command, finalDestinationPoint);
        LOG.info(
            "{}: Received final-destination MQTT feedback while current route step is {}. "
                + "Completing current step locally and caching final feedback: order={} final={}",
            getName(),
            destinationPoint,
            command.getTransportOrder().getName(),
            finalDestinationPoint
        );
        completeNavigationCommand(command, destinationPoint);
        return;
      }
      LOG.debug(
          "{}: Ignoring success feedback not matching current destination {}.",
          getName(),
          destinationPoint
      );
      LOG.info(
          "{}: MQTT success did not match current command point: feedbackPoint={} pointId={} "
              + "commandPoint={} order={}",
          getName(),
          feedback.pointName(),
          feedback.pointId(),
          destinationPoint,
          command.getTransportOrder().getName()
      );
      return;
    }

    completeNavigationCommand(command, destinationPoint);
  }

  /**
   * Completes a navigation step, or starts the physical operation attached to
   * that step. openTCS may represent a destination operation (notably DROP)
   * on the same path-bearing MovementCommand instead of emitting a separate
   * no-path command. In that case the lift action must be started only after
   * navigation feedback succeeds.
   */
  private void completeNavigationCommand(MovementCommand command, String destinationPoint) {
    if (isLiftOperation(command.getOperation())) {
      if (Objects.equals(pendingSpecialPointNavigationCommand, command)) {
        pendingSpecialPointNavigationCommand = null;
        cancelCommandTimeout();
      }
      if (isSpecialStoragePoint(destinationPoint)) {
        startMagneticTransit(command, 1);
        return;
      }
      startLiftOperation(command);
      return;
    }
    finishMovementCommand(command, destinationPoint);
  }

  private void finishMovementCommand(MovementCommand command, String destinationPoint) {
    cancelCommandTimeout();
    if (getSentCommands().size() <= 1 && getUnsentCommands().isEmpty()) {
      getProcessModel().setState(Vehicle.State.IDLE);
    }
    getProcessModel().setPosition(destinationPoint);

    if (Objects.equals(getSentCommands().peek(), command)) {
      getProcessModel().commandExecuted(getSentCommands().poll());
      clearCachedFinalFeedbackIfFinal(command);
      scheduleIdleReturn();
    }
    else {
      LOG.warn(
          "{}: MQTT feedback command is not oldest in sent queue: {} != {}",
          getName(),
          command,
          getSentCommands().peek()
      );
    }
  }

  private void failCurrentCommand(FeedbackMessage feedback) {
    MovementCommand command = getSentCommands().peek();
    if (command == null) {
      LOG.debug("{}: Ignoring failure feedback without a current command.", getName());
      return;
    }

    String destinationPoint = command.getStep().getDestinationPoint().getName();
    if (!feedbackMatchesCommand(feedback, destinationPoint)) {
      return;
    }

    if (isNavigationCommand(command)) {
      publishStopForCommand(command);
    }
    getProcessModel().commandFailed(command);
    getProcessModel().setState(Vehicle.State.IDLE);
    clearPendingLiftOperation(command);
    pendingMagneticTransitCommand = null;
    pendingMagneticTransitAction = 0;
    pendingPostDropNavigationCommand = null;
    cancelCommandTimeout();
    LOG.warn(
        "{}: AGV reported movement failure destination={} status={}",
        getName(),
        destinationPoint,
        feedback.status()
    );
  }

  private void failPendingLiftOperation(FeedbackMessage feedback) {
    MovementCommand command = pendingLiftOperationCommand;
    if (command == null) {
      return;
    }
    getProcessModel().commandFailed(command);
    getProcessModel().setState(Vehicle.State.IDLE);
    clearPendingLiftOperation(command);
    pendingMagneticTransitCommand = null;
    pendingMagneticTransitAction = 0;
    cancelCommandTimeout();
    LOG.warn(
        "{}: AGV magnetic_nav operation failed: operation={} status={}",
        getName(),
        command.getOperation(),
        feedback.status()
    );
  }

  /**
   * Fails whatever command is currently in flight when the AGV reports its
   * navigation error mode (base_status status=9). The AGV will not complete
   * the current action on its own, so waiting for the phase timeout only
   * delays the (inevitable) order failure.
   */
  private void failCurrentCommandOnNavError(FeedbackMessage feedback) {
    String errorDetail = "navStatus=" + feedback.navStatus()
        + ", robotStatus=" + feedback.robotStatus()
        + ", mainError=" + feedback.mainError()
        + ", subError=" + feedback.subError();

    MovementCommand liftCommand = pendingLiftOperationCommand;
    if (liftCommand != null) {
      getProcessModel().commandFailed(liftCommand);
      getProcessModel().setState(Vehicle.State.IDLE);
      clearPendingLiftOperation(liftCommand);
      pendingMagneticTransitCommand = null;
      pendingMagneticTransitAction = 0;
      pendingPostDropNavigationCommand = null;
      cancelCommandTimeout();
      LOG.warn(
          "{}: Failing {} operation because AGV entered error state: order={} {}",
          getName(),
          liftCommand.getOperation(),
          liftCommand.getTransportOrder().getName(),
          errorDetail
      );
      return;
    }

    MovementCommand transitCommand = pendingMagneticTransitCommand;
    if (transitCommand != null) {
      pendingMagneticTransitCommand = null;
      pendingMagneticTransitAction = 0;
      cancelCommandTimeout();
      getProcessModel().commandFailed(transitCommand);
      getProcessModel().setState(Vehicle.State.IDLE);
      LOG.warn(
          "{}: Failing magnetic transit of {} because AGV entered error state: order={} {}",
          getName(),
          transitCommand.getOperation(),
          transitCommand.getTransportOrder().getName(),
          errorDetail
      );
      return;
    }

    MovementCommand postDropCommand = pendingPostDropNavigationCommand;
    if (postDropCommand != null) {
      pendingPostDropNavigationCommand = null;
      cancelCommandTimeout();
      publishStopForCommand(postDropCommand);
      getProcessModel().commandFailed(postDropCommand);
      getProcessModel().setState(Vehicle.State.IDLE);
      LOG.warn(
          "{}: Failing post-DROP navigation to {} because AGV entered error state: order={} {}",
          getName(),
          postDropCommand.getStep().getDestinationPoint().getName(),
          postDropCommand.getTransportOrder().getName(),
          errorDetail
      );
      return;
    }

    MovementCommand command = getSentCommands().peek();
    if (command == null) {
      LOG.warn(
          "{}: AGV entered navigation error state with no command in flight: {}",
          getName(),
          errorDetail
      );
      return;
    }
    if (isNavigationCommand(command)) {
      publishStopForCommand(command);
    }
    getProcessModel().commandFailed(command);
    getProcessModel().setState(Vehicle.State.IDLE);
    clearPendingLiftOperation(command);
    cancelCommandTimeout();
    LOG.warn(
        "{}: Failing command to {} because AGV entered error state: order={} {}",
        getName(),
        command.getStep().getDestinationPoint().getName(),
        command.getTransportOrder().getName(),
        errorDetail
    );
  }

  private boolean feedbackMatchesCommand(FeedbackMessage feedback, String destinationPoint) {
    if (feedback.pointName() != null) {
      return pointNamesEquivalent(feedback.pointName(), destinationPoint);
    }

    if (feedback.pointId() == null) {
      return true;
    }

    Optional<Integer> expectedPointId = resolvePointId(destinationPoint);
    return expectedPointId.isPresent() && expectedPointId.get().equals(feedback.pointId());
  }

  private boolean pointNamesEquivalent(String feedbackPoint, String commandPoint) {
    if (Objects.equals(feedbackPoint, commandPoint)) {
      return true;
    }
    if (feedbackPoint == null || commandPoint == null) {
      return false;
    }
    if (commandPoint.startsWith("LOC_")) {
      return Objects.equals(commandPoint.substring("LOC_".length()), feedbackPoint);
    }
    if (feedbackPoint.startsWith("LOC_")) {
      return Objects.equals(feedbackPoint.substring("LOC_".length()), commandPoint);
    }
    return false;
  }

  private boolean isLiftOperation(String operation) {
    String normalized = normalizedOperation(operation);
    return isLiftUpOperation(normalized) || isLiftDownOperation(normalized);
  }

  private boolean isLiftUpOperation(String operation) {
    String normalized = normalizedOperation(operation);
    return normalized.equals(OPERATION_PICK)
        || normalized.equals(OPERATION_LOAD)
        || normalized.equals(OPERATION_LIFT_UP);
  }

  private boolean isLiftDownOperation(String operation) {
    String normalized = normalizedOperation(operation);
    return normalized.equals(OPERATION_DROP)
        || normalized.equals(OPERATION_UNLOAD)
        || normalized.equals(OPERATION_LIFT_DOWN);
  }

  private int liftAction(String operation) {
    if (isLiftUpOperation(operation)) {
      return 3;
    }
    if (isLiftDownOperation(operation)) {
      return 4;
    }
    throw new IllegalArgumentException("Unsupported lift operation: " + operation);
  }

  private String normalizedOperation(String operation) {
    if (operation == null) {
      return "";
    }
    return operation.trim().toUpperCase(Locale.ROOT).replace('-', '_');
  }

  private boolean liftCompletionConditionsSatisfied() {
    if (!hasMagneticStatus || !liftStateMatches(pendingLiftOperation)) {
      return false;
    }
    // A command that was sent requires both the AGV action confirmation and
    // a fresh physical sensor state observed after the command was issued.
    if (liftCommandPublished && !liftTaskSuccess) {
      return false;
    }
    return true;
  }

  private void completePendingLiftOperation() {
    if (liftCompletionScheduled) {
      return;
    }
    liftCompletionScheduled = true;
    MovementCommand command = pendingLiftOperationCommand;
    String operation = pendingLiftOperation;
    getExecutor().schedule(
        () -> {
          synchronized (MqttCommAdapter.this) {
            if (!Objects.equals(pendingLiftOperationCommand, command)) {
              return;
            }
            clearPendingLiftOperation(command);
            setLoadHandlingDeviceLoaded(isLiftUpOperation(operation));
            if (isMagneticStoragePoint(command.getStep().getDestinationPoint().getName())) {
              startMagneticTransit(command, 2);
            }
            else {
              finishMovementCommand(command, command.getStep().getDestinationPoint().getName());
            }
          }
        },
        settings.liftSettleTimeMillis(),
        TimeUnit.MILLISECONDS
    );
  }

  private boolean isSpecialStoragePoint(String pointName) {
    return isMagneticStoragePoint(pointName);
  }

  private boolean isMagneticStoragePoint(String pointName) {
    return resolvePointId(pointName).orElse(-1) > 0;
  }

  private void startMagneticTransit(MovementCommand command, int action) {
    pendingMagneticTransitCommand = command;
    pendingMagneticTransitAction = action;
    Map<String, Object> payload = new LinkedHashMap<>();
    payload.put("cmd_type", "magnetic_nav");
    payload.put("aim_id", 0);
    payload.put("aim_dir", 0);
    payload.put("aim_action", action);
    payload.put("opentcs_vehicle", getName());
    payload.put("opentcs_operation", action == 1 ? "MAGNETIC_FORWARD" : "MAGNETIC_BACKWARD");
    publishCommand(toJson(payload));
    scheduleCommandTimeout(
        command, settings.magneticTransitTimeoutSeconds(),
        action == 1 ? "magnetic forward" : "magnetic backward"
    );
    LOG.info(
        "{}: Published magnetic transit command action={} for operation={}",
        getName(), action, command.getOperation()
    );
  }

  private void startPostDropNavigation(MovementCommand command) {
    String pointName = command.getStep().getDestinationPoint().getName();
    int pointId = resolvePointId(pointName)
        .orElseThrow(
            () -> new IllegalArgumentException("No MQTT point id mapping for point: " + pointName)
        );
    pendingPostDropNavigationCommand = command;
    publishCommand(toJson(buildCommandPayload(command, pointName, pointName, pointId)));
    scheduleCommandTimeout(command, settings.navigationTimeoutSeconds(), "post-drop navigation");
    getProcessModel().setState(Vehicle.State.EXECUTING);
    LOG.info(
        "{}: Published post-DROP navigation confirmation to point={} id={}",
        getName(), pointName, pointId
    );
  }

  private void failMagneticTransit(FeedbackMessage feedback) {
    MovementCommand command = pendingMagneticTransitCommand;
    if (command == null) {
      return;
    }
    pendingMagneticTransitCommand = null;
    pendingMagneticTransitAction = 0;
    cancelCommandTimeout();
    getProcessModel().commandFailed(command);
    getProcessModel().setState(Vehicle.State.IDLE);
    LOG.warn(
        "{}: Magnetic transit failed: operation={} status={}",
        getName(), command.getOperation(), feedback.status()
    );
  }

  private void completePostDropNavigation(FeedbackMessage feedback) {
    MovementCommand command = pendingPostDropNavigationCommand;
    if (command == null || !isNavigationFeedback(feedback)) {
      return;
    }
    String pointName = command.getStep().getDestinationPoint().getName();
    if (!feedbackMatchesCommand(feedback, pointName)) {
      return;
    }
    pendingPostDropNavigationCommand = null;
    finishMovementCommand(command, pointName);
  }

  private void failPostDropNavigation(FeedbackMessage feedback) {
    MovementCommand command = pendingPostDropNavigationCommand;
    if (command == null || !isNavigationFeedback(feedback)) {
      return;
    }
    String pointName = command.getStep().getDestinationPoint().getName();
    if (!feedbackMatchesCommand(feedback, pointName)) {
      return;
    }
    pendingPostDropNavigationCommand = null;
    cancelCommandTimeout();
    publishStopForCommand(command);
    getProcessModel().commandFailed(command);
    getProcessModel().setState(Vehicle.State.IDLE);
    LOG.warn(
        "{}: Post-DROP navigation failed: destination={} status={}",
        getName(), pointName, feedback.status()
    );
  }

  private boolean isNavigationFeedback(FeedbackMessage feedback) {
    return feedback.type() == null
        || feedback.type().isBlank()
        || feedback.type().equalsIgnoreCase("nav")
        || feedback.type().equalsIgnoreCase("interest_point_control");
  }

  private synchronized void scheduleIdleReturn() {
    cancelIdleReturn();
    if (settings.idleReturnSeconds() <= 0 || !isVehicleConnected()) {
      return;
    }
    idleReturnFuture = getExecutor().schedule(
        () -> {
          synchronized (MqttCommAdapter.this) {
            if (!getSentCommands().isEmpty()
                || !getUnsentCommands().isEmpty()
                || getProcessModel().getState() != Vehicle.State.IDLE) {
              scheduleIdleReturn();
              return;
            }
            String standbyPoint = pointNamesByFeedbackKey.get(
                String.valueOf(settings.standbyPointId())
            );
            if (standbyPoint == null) {
              LOG.warn(
                  "{}: Cannot return to standby point id={}, no point mapping.", getName(), settings
                      .standbyPointId()
              );
              return;
            }
            if (Objects.equals(getProcessModel().getPosition(), standbyPoint)) {
              scheduleIdleReturn();
              return;
            }
            idleReturnActive = true;
            idleReturnPointName = standbyPoint;
            Map<String, Object> payload = new LinkedHashMap<>();
            payload.put("cmd_type", "interest_point_control");
            payload.put("cmd", "start");
            payload.put("id", settings.standbyPointId());
            payload.put("run_speed", settings.runSpeed());
            payload.put("path_stop_time", 0);
            payload.put("path_mode", 2);
            payload.put("circulates", 0);
            payload.put("time", 0);
            payload.put("opentcs_vehicle", getName());
            payload.put("opentcs_operation", "IDLE_RETURN");
            publishCommand(toJson(payload));
            getProcessModel().setState(Vehicle.State.EXECUTING);
            scheduleIdleReturnTimeout();
            LOG.info(
                "{}: Idle timeout reached, returning to standby point={} id={}", getName(),
                standbyPoint, settings.standbyPointId()
            );
          }
        },
        settings.idleReturnSeconds(),
        TimeUnit.SECONDS
    );
  }

  private synchronized void scheduleIdleReturnTimeout() {
    idleReturnFuture = getExecutor().schedule(
        () -> {
          synchronized (MqttCommAdapter.this) {
            if (!idleReturnActive) {
              return;
            }
            publishIdleReturnStop();
            idleReturnActive = false;
            idleReturnPointName = null;
            getProcessModel().setState(Vehicle.State.IDLE);
            scheduleIdleReturn();
            LOG.warn("{}: Return to standby point timed out.", getName());
          }
        },
        settings.navigationTimeoutSeconds(),
        TimeUnit.SECONDS
    );
  }

  private void completeIdleReturn(FeedbackMessage feedback) {
    if (!isNavigationFeedback(feedback)
        || !feedbackMatchesCommand(feedback, idleReturnPointName)) {
      return;
    }
    cancelIdleReturn();
    getProcessModel().setPosition(idleReturnPointName);
    idleReturnActive = false;
    idleReturnPointName = null;
    getProcessModel().setState(Vehicle.State.IDLE);
    scheduleIdleReturn();
    LOG.info("{}: Returned to standby point successfully.", getName());
  }

  private void failIdleReturn(FeedbackMessage feedback) {
    if (!isNavigationFeedback(feedback)) {
      return;
    }
    cancelIdleReturn();
    publishIdleReturnStop();
    idleReturnActive = false;
    idleReturnPointName = null;
    getProcessModel().setState(Vehicle.State.IDLE);
    scheduleIdleReturn();
    LOG.warn("{}: Return to standby point failed: status={}", getName(), feedback.status());
  }

  private void publishIdleReturnStop() {
    Map<String, Object> payload = new LinkedHashMap<>();
    payload.put("circulates", 0);
    payload.put("cmd", "stop");
    payload.put("cmd_type", "interest_point_control");
    payload.put("id", settings.standbyPointId());
    payload.put("path_mode", 2);
    payload.put("path_stop_time", 0);
    payload.put("run_speed", settings.runSpeed());
    payload.put("time", 1);
    publishCommand(toJson(payload));
  }

  private synchronized void cancelIdleReturn() {
    if (idleReturnFuture != null) {
      idleReturnFuture.cancel(false);
      idleReturnFuture = null;
    }
  }

  private boolean isNavigationCommand(MovementCommand command) {
    return (command.getStep().getPath() != null && !isLiftOperation(command.getOperation()))
        || Objects.equals(pendingSpecialPointNavigationCommand, command)
        || Objects.equals(pendingPostDropNavigationCommand, command);
  }

  private boolean isNavigationCommandCandidate(MovementCommand command) {
    return command.getStep().getPath() != null || isLiftOperation(command.getOperation());
  }

  private void publishStopForCommand(MovementCommand command) {
    if (!isVehicleConnected()) {
      return;
    }
    String destinationPoint = command.getStep().getDestinationPoint().getName();
    Optional<Integer> pointId = resolvePointId(destinationPoint);
    if (pointId.isEmpty()) {
      LOG.warn(
          "{}: Cannot publish navigation stop, no point id for {}", getName(), destinationPoint
      );
      return;
    }
    Map<String, Object> payload = new LinkedHashMap<>();
    payload.put("circulates", 0);
    payload.put("cmd", "stop");
    payload.put("cmd_type", "interest_point_control");
    payload.put("id", pointId.get());
    payload.put("path_mode", 2);
    payload.put("path_stop_time", 0);
    payload.put("run_speed", settings.runSpeed());
    payload.put("time", 1);
    publishCommand(toJson(payload));
    LOG.info(
        "{}: Published navigation stop for point={} id={}", getName(), destinationPoint, pointId
            .get()
    );
  }

  private synchronized void scheduleCommandTimeout(
      MovementCommand command,
      int timeoutSeconds,
      String phase
  ) {
    cancelCommandTimeout();
    if (timeoutSeconds <= 0) {
      return;
    }
    commandTimeoutFuture = getExecutor().schedule(
        () -> {
          synchronized (MqttCommAdapter.this) {
            boolean active = Objects.equals(getSentCommands().peek(), command)
                || Objects.equals(pendingLiftOperationCommand, command)
                || Objects.equals(pendingMagneticTransitCommand, command)
                || Objects.equals(pendingSpecialPointNavigationCommand, command)
                || Objects.equals(pendingPostDropNavigationCommand, command);
            if (!active) {
              return;
            }
            LOG.warn(
                "{}: {} command timed out after {} seconds: order={} operation={} destination={}",
                getName(),
                phase,
                timeoutSeconds,
                command.getTransportOrder().getName(),
                command.getOperation(),
                command.getStep().getDestinationPoint().getName()
            );
            clearPendingLiftOperation(command);
            if (Objects.equals(pendingMagneticTransitCommand, command)) {
              pendingMagneticTransitCommand = null;
              pendingMagneticTransitAction = 0;
            }
            if (Objects.equals(pendingSpecialPointNavigationCommand, command)) {
              pendingSpecialPointNavigationCommand = null;
            }
            if (Objects.equals(pendingPostDropNavigationCommand, command)) {
              pendingPostDropNavigationCommand = null;
            }
            cachedFinalFeedbackOrderName = null;
            cachedFinalFeedbackPointName = null;
            if (isNavigationCommand(command)) {
              publishStopForCommand(command);
            }
            getProcessModel().commandFailed(command);
            getProcessModel().setState(Vehicle.State.IDLE);
            cancelCommandTimeout();
          }
        },
        timeoutSeconds,
        TimeUnit.SECONDS
    );
  }

  private synchronized void cancelCommandTimeout() {
    if (commandTimeoutFuture != null) {
      commandTimeoutFuture.cancel(false);
      commandTimeoutFuture = null;
    }
  }

  private void publishTransitFollowupNavigation(MovementCommand command) {
    String pointName = command.getStep().getDestinationPoint().getName();
    int pointId = resolvePointId(pointName)
        .orElseThrow(
            () -> new IllegalArgumentException("No MQTT point id mapping for point: " + pointName)
        );
    publishCommand(toJson(buildCommandPayload(command, pointName, pointName, pointId)));
    getProcessModel().setState(Vehicle.State.EXECUTING);
    LOG.info(
        "{}: Published follow-up navigation confirmation to point={} id={}",
        getName(), pointName, pointId
    );
  }

  private void startLiftOperation(MovementCommand command) {
    String operation = normalizedOperation(command.getOperation());
    if (Objects.equals(pendingLiftOperationCommand, command)) {
      LOG.warn(
          "{}: Ignoring duplicate lift command request: operation={} order={}",
          getName(),
          operation,
          command.getTransportOrder().getName()
      );
      return;
    }
    pendingLiftOperationCommand = command;
    pendingLiftOperation = operation;
    liftCompletionScheduled = false;
    liftTaskSuccess = false;
    liftCommandPublished = false;
    // Invalidate the previous base_status snapshot. Otherwise a stale
    // material/up/down state from before this operation could complete the
    // command as soon as magnetic_nav reports success.
    hasMagneticStatus = false;
    // Always issue PICK/LIFT_UP and DROP/LIFT_DOWN explicitly. A base_status
    // snapshot may be stale (for example, material=false before a DROP), and
    // it must not be treated as proof that the current physical action ran.
    publishLiftOperationCommand(command);
    scheduleCommandTimeout(command, settings.liftTimeoutSeconds(), "lift " + operation);
  }

  private boolean liftStateMatches(String operation) {
    if (isLiftUpOperation(operation)) {
      return materialPresent && liftUp;
    }
    if (isLiftDownOperation(operation)) {
      // DROP is complete only once the AGV has confirmed the lift command, the
      // load sensor reports that no material remains on the forks and the
      // forks have physically returned to their lowered position (magnetic
      // "down"/"low" sensor). material=false alone is not sufficient: the load
      // may already be gone while the forks are still raised.
      return !materialPresent && liftDown;
    }
    return false;
  }

  private void clearPendingLiftOperation(MovementCommand command) {
    if (Objects.equals(pendingLiftOperationCommand, command)) {
      pendingLiftOperationCommand = null;
      pendingLiftOperation = null;
      liftCompletionScheduled = false;
      liftTaskSuccess = false;
      liftCommandPublished = false;
    }
  }

  private void setLoadHandlingDeviceLoaded(boolean loaded) {
    getProcessModel().setLoadHandlingDevices(
        java.util.List.of(new LoadHandlingDevice(LHD_NAME, loaded))
    );
  }

  private void cacheFinalFeedback(MovementCommand command, String finalDestinationPoint) {
    cachedFinalFeedbackOrderName = command.getTransportOrder().getName();
    cachedFinalFeedbackPointName = finalDestinationPoint;
  }

  private boolean hasCachedFinalFeedbackFor(MovementCommand command) {
    return cachedFinalFeedbackOrderName != null
        && cachedFinalFeedbackPointName != null
        && Objects.equals(cachedFinalFeedbackOrderName, command.getTransportOrder().getName())
        && Objects.equals(cachedFinalFeedbackPointName, command.getFinalDestination().getName());
  }

  private void clearCachedFinalFeedbackIfFinal(MovementCommand command) {
    if (command.isFinalMovement() && hasCachedFinalFeedbackFor(command)) {
      cachedFinalFeedbackOrderName = null;
      cachedFinalFeedbackPointName = null;
    }
  }

  private Optional<Integer> resolvePointId(String pointName) {
    Integer mappedId = pointIdsByName.get(pointName);
    if (mappedId != null) {
      return Optional.of(mappedId);
    }

    Matcher matcher = TRAILING_NUMBER_PATTERN.matcher(pointName);
    if (matcher.find()) {
      return Optional.of(Integer.parseInt(matcher.group(1)));
    }
    return Optional.empty();
  }

  private void rebuildPointMappings(String rawMapping) {
    pointIdsByName.clear();
    pointNamesByFeedbackKey.clear();

    if (rawMapping == null || rawMapping.isBlank()) {
      return;
    }

    for (String entry : rawMapping.split("[,;\\r\\n]+")) {
      String trimmed = entry.trim();
      if (trimmed.isBlank() || !trimmed.contains("=")) {
        continue;
      }
      String[] parts = trimmed.split("=", 2);
      String left = parts[0].trim();
      String right = parts[1].trim();

      parseLegacyFeedbackMapping(left, right);
      parsePointNameToIdMapping(left, right);
    }
  }

  private void parseLegacyFeedbackMapping(String left, String right) {
    String[] typeAndId = left.split(":", 2);
    if (typeAndId.length != 2 || !isInteger(typeAndId[1]) || right.isBlank()) {
      return;
    }

    int pointId = Integer.parseInt(typeAndId[1]);
    String type = typeAndId[0].trim().toLowerCase(Locale.ROOT);
    pointIdsByName.put(right, pointId);
    pointNamesByFeedbackKey.put(type + ":" + pointId, right);
    pointNamesByFeedbackKey.putIfAbsent(String.valueOf(pointId), right);
  }

  private void parsePointNameToIdMapping(String left, String right) {
    if (left.isBlank() || !isInteger(right)) {
      return;
    }

    int pointId = Integer.parseInt(right);
    pointIdsByName.put(left, pointId);
    pointNamesByFeedbackKey.putIfAbsent(String.valueOf(pointId), left);
  }

  private boolean isInteger(String value) {
    if (value == null || value.isBlank()) {
      return false;
    }
    try {
      Integer.parseInt(value.trim());
      return true;
    }
    catch (NumberFormatException exc) {
      return false;
    }
  }

  private String pointNameForFeedback(String type, Integer pointId) {
    if (pointId == null) {
      return null;
    }
    if (type != null) {
      String withType = pointNamesByFeedbackKey.get(
          type.trim().toLowerCase(Locale.ROOT) + ":" + pointId
      );
      if (withType != null) {
        return withType;
      }
    }
    return pointNamesByFeedbackKey.get(String.valueOf(pointId));
  }

  private class AdapterMqttCallback
      implements
        MqttCallbackExtended {

    AdapterMqttCallback() {
    }

    @Override
    public void connectComplete(boolean reconnect, String serverURI) {
      getProcessModel().setCommAdapterConnected(true);
      try {
        subscribeFeedbackTopics();
      }
      catch (MqttException exc) {
        LOG.warn("{}: Could not subscribe feedback topics: {}", getName(), exc.getMessage());
      }
    }

    @Override
    public void connectionLost(Throwable cause) {
      getProcessModel().setCommAdapterConnected(false);
      LOG.warn(
          "{}: MQTT connection lost: {}",
          getName(),
          cause == null ? "<unknown>" : cause.getMessage()
      );
    }

    @Override
    public void messageArrived(String topic, MqttMessage message) {
      String payloadJson = new String(message.getPayload(), StandardCharsets.UTF_8);
      getExecutor().execute(() -> handleFeedback(topic, payloadJson));
    }

    @Override
    public void deliveryComplete(IMqttDeliveryToken token) {
    }
  }

  record FeedbackMessage(
      String cmdType,
      String agvId,
      String type,
      Integer pointId,
      String pointName,
      String status,
      Integer energyLevel,
      Boolean material,
      Boolean up,
      Boolean down,
      Integer mainError,
      Integer subError,
      Integer robotStatus,
      Integer navStatus,
      Double amcl,
      Boolean location
  ) {

    static FeedbackMessage parse(JsonNode root) {
      String status = firstText(root, "status", "state", "result", "event_type", "eventType");
      String type = firstText(root, "type", "task_type", "taskType");
      Integer batteryLevel = firstInt(
          root,
          "battery",
          "battery_level",
          "batteryLevel",
          "energy",
          "energyLevel"
      );
      if (batteryLevel == null) {
        batteryLevel = validBmsSoc(root.path("bms"));
      }
      Boolean material = firstBoolean(root.path("magnetic"), "material");
      Boolean up = firstBoolean(root.path("magnetic"), "up");
      Boolean down = firstBoolean(root.path("magnetic"), "down", "low");
      Integer mainError = firstInt(root.path("robot"), "mainerror", "mainError");
      Integer subError = firstInt(root.path("robot"), "suberror", "subError");
      Integer robotStatus = firstInt(root.path("robot"), "robot_status", "robotStatus");
      // base_status carries the navigation system mode in "status"
      // (0 idle, 1 navigating, ..., 9 error). task_feedback uses the same
      // field with word values (success/failed/...), which do not parse.
      Integer navStatus = firstInt(root, "status", "nav_status", "navStatus", "mode");
      JsonNode local = root.path("local");
      Double amcl = local.path("amcl").isNumber() ? local.path("amcl").asDouble() : null;
      Boolean location = firstBoolean(local, "location");
      if (material == null) {
        material = firstBoolean(root, "material");
      }
      if (up == null) {
        up = firstBoolean(root, "up");
      }
      if (down == null) {
        down = firstBoolean(root, "down");
      }
      return new FeedbackMessage(
          normalizedText(root, "cmd_type", "cmdType"),
          firstText(root, "agv_id", "agvId", "vehicle_id", "vehicleId"),
          type,
          firstInt(root, "id", "point_id", "pointId", "current_point", "currentPoint"),
          firstText(root, "point_name", "pointName", "opentcs_point", "opentcsPoint"),
          status == null ? "" : status.trim().toLowerCase(Locale.ROOT),
          batteryLevel,
          material,
          up,
          down,
          mainError,
          subError,
          robotStatus,
          navStatus,
          amcl,
          location
      );
    }

    boolean isSupportedFeedback() {
      return cmdType == null
          || cmdType.isBlank()
          || cmdType.equals("task_feedback")
          || cmdType.equals("base_status");
    }

    boolean hasMagneticStatus() {
      return material != null && up != null && down != null;
    }

    boolean hasLocalization() {
      return amcl != null && location != null;
    }

    boolean matchesAgv(String expectedAgvId) {
      return expectedAgvId == null
          || expectedAgvId.isBlank()
          || agvId == null
          || Objects.equals(expectedAgvId, agvId);
    }

    boolean isProgress() {
      return status.equals("start")
          || status.equals("process")
          || status.equals("processing")
          || status.equals("running")
          || status.equals("try");
    }

    boolean isSuccess() {
      return status.equals("success")
          || status.equals("succeeded")
          || status.equals("done")
          || status.equals("completed")
          || status.equals("complete");
    }

    boolean isFailure() {
      return status.equals("failure")
          || status.equals("failed")
          || status.equals("fail")
          || status.equals("timeout")
          || status.equals("error");
    }

    /**
     * Returns whether the AGV reports its navigation error mode. Only
     * base_status mode 9 indicates an error; the robot mainerror/suberror
     * fields alone do not (the AGV may report them while operating).
     */
    boolean isNavError() {
      return "base_status".equals(cmdType)
          && navStatus != null
          && navStatus == NAV_STATUS_ERROR;
    }

    private static String firstText(JsonNode node, String... keys) {
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

    private static String normalizedText(JsonNode node, String... keys) {
      String value = firstText(node, keys);
      return value == null ? null : value.toLowerCase(Locale.ROOT);
    }

    private static Integer firstInt(JsonNode node, String... keys) {
      for (String key : keys) {
        JsonNode child = node.path(key);
        if (child.isInt()) {
          return child.asInt();
        }
        if (child.isTextual()) {
          try {
            return Integer.parseInt(child.asText().trim());
          }
          catch (NumberFormatException exc) {
            return null;
          }
        }
      }
      return null;
    }

    /**
     * Reads SOC from BMS data, ignoring the all-zero placeholder reported by some AGVs when
     * their battery interface is unavailable. A real SOC of zero is retained if other BMS
     * fields contain meaningful values.
     */
    private static Integer validBmsSoc(JsonNode bms) {
      Integer soc = firstInt(bms, "soc");
      if (soc == null) {
        return null;
      }
      double voltage = bms.path("voltage").asDouble(0.0);
      Integer remainingCapacityValue = firstInt(bms, "remaining_capacity", "remainingCapacity");
      int remainingCapacity = remainingCapacityValue == null ? 0 : remainingCapacityValue;
      Integer statusValue = firstInt(bms, "status");
      int status = statusValue == null ? 0 : statusValue;
      if (soc == 0 && voltage == 0.0 && remainingCapacity == 0 && status == 0) {
        return null;
      }
      return Math.max(0, Math.min(100, soc));
    }

    private static Boolean firstBoolean(JsonNode node, String... keys) {
      for (String key : keys) {
        JsonNode child = node.path(key);
        if (child.isBoolean()) {
          return child.asBoolean();
        }
        if (child.isTextual()) {
          String value = child.asText().trim().toLowerCase(Locale.ROOT);
          if (value.equals("true") || value.equals("1") || value.equals("yes")) {
            return true;
          }
          if (value.equals("false") || value.equals("0") || value.equals("no")) {
            return false;
          }
        }
      }
      return null;
    }
  }

  private record MqttSettings(
      String brokerUri,
      String commandTopic,
      String feedbackTopic,
      String baseStatusTopic,
      String pointIdMap,
      String initialPosition,
      String agvId,
      String username,
      String password,
      String clientId,
      int qos,
      double runSpeed,
      int connectionTimeoutSeconds,
      int keepAliveSeconds,
      long liftSettleTimeMillis,
      int navigationTimeoutSeconds,
      int liftTimeoutSeconds,
      int magneticTransitTimeoutSeconds,
      int idleReturnSeconds,
      int standbyPointId,
      double minAmclScore
  ) {

    static MqttSettings from(Map<String, String> vehicleProperties) {
      return new MqttSettings(
          read(
              vehicleProperties,
              PROP_BROKER_URI,
              "opentcs.mqtt.brokerUri",
              "tcp://192.168.10.209:1883"
          ),
          read(
              vehicleProperties,
              PROP_COMMAND_TOPIC,
              "opentcs.mqtt.commandTopic",
              "robot_control"
          ),
          read(
              vehicleProperties,
              PROP_FEEDBACK_TOPIC,
              "opentcs.mqtt.feedbackTopic",
              "task_feedback"
          ),
          read(
              vehicleProperties,
              PROP_BASE_STATUS_TOPIC,
              "opentcs.mqtt.baseStatusTopic",
              "base_status"
          ),
          read(vehicleProperties, PROP_POINT_ID_MAP, "opentcs.mqtt.pointIdMap", ""),
          read(vehicleProperties, PROP_INITIAL_POSITION, "opentcs.mqtt.initialPosition", ""),
          read(vehicleProperties, PROP_AGV_ID, "opentcs.mqtt.agvId", ""),
          read(vehicleProperties, "mqtt:username", "opentcs.mqtt.username", ""),
          read(vehicleProperties, "mqtt:password", "opentcs.mqtt.password", ""),
          read(vehicleProperties, "mqtt:clientId", "opentcs.mqtt.clientId", ""),
          readInt(vehicleProperties, "mqtt:qos", "opentcs.mqtt.qos", 0),
          readDouble(vehicleProperties, "mqtt:runSpeed", "opentcs.mqtt.runSpeed", 0.5),
          readInt(
              vehicleProperties,
              "mqtt:connectionTimeoutSeconds",
              "opentcs.mqtt.connectionTimeoutSeconds",
              3
          ),
          readInt(vehicleProperties, "mqtt:keepAliveSeconds", "opentcs.mqtt.keepAliveSeconds", 10),
          readLong(
              vehicleProperties,
              PROP_LIFT_SETTLE_TIME,
              "opentcs.mqtt.liftSettleTimeMillis",
              1000L
          ),
          readInt(
              vehicleProperties,
              "mqtt:navigationTimeoutSeconds",
              "opentcs.mqtt.navigationTimeoutSeconds",
              120
          ),
          readInt(
              vehicleProperties,
              "mqtt:liftTimeoutSeconds",
              "opentcs.mqtt.liftTimeoutSeconds",
              300
          ),
          readInt(
              vehicleProperties,
              "mqtt:magneticTransitTimeoutSeconds",
              "opentcs.mqtt.magneticTransitTimeoutSeconds",
              60
          ),
          readInt(
              vehicleProperties,
              "mqtt:idleReturnSeconds",
              "opentcs.mqtt.idleReturnSeconds",
              300
          ),
          readInt(
              vehicleProperties,
              "mqtt:standbyPointId",
              "opentcs.mqtt.standbyPointId",
              0
          ),
          readDouble(
              vehicleProperties,
              "mqtt:minAmclScore",
              "opentcs.mqtt.minAmclScore",
              0.5
          )
      );
    }

    String clientId(String vehicleName) {
      if (!clientId.isBlank()) {
        return clientId;
      }
      return "opentcs-mqtt-" + vehicleName + "-" + System.currentTimeMillis();
    }

    private static String read(
        Map<String, String> vehicleProperties,
        String propertyKey,
        String systemPropertyKey,
        String defaultValue
    ) {
      String vehicleValue = vehicleProperties.get(propertyKey);
      if (vehicleValue != null && !vehicleValue.isBlank()) {
        return vehicleValue.trim();
      }

      String systemValue = System.getProperty(systemPropertyKey);
      if (systemValue != null && !systemValue.isBlank()) {
        return systemValue.trim();
      }
      return defaultValue;
    }

    private static int readInt(
        Map<String, String> vehicleProperties,
        String propertyKey,
        String systemPropertyKey,
        int defaultValue
    ) {
      try {
        return Integer.parseInt(read(vehicleProperties, propertyKey, systemPropertyKey, ""));
      }
      catch (NumberFormatException exc) {
        return defaultValue;
      }
    }

    private static double readDouble(
        Map<String, String> vehicleProperties,
        String propertyKey,
        String systemPropertyKey,
        double defaultValue
    ) {
      try {
        return Double.parseDouble(read(vehicleProperties, propertyKey, systemPropertyKey, ""));
      }
      catch (NumberFormatException exc) {
        return defaultValue;
      }
    }

    private static long readLong(
        Map<String, String> vehicleProperties,
        String propertyKey,
        String systemPropertyKey,
        long defaultValue
    ) {
      try {
        return Long.parseLong(read(vehicleProperties, propertyKey, systemPropertyKey, ""));
      }
      catch (NumberFormatException exc) {
        return defaultValue;
      }
    }
  }
}
