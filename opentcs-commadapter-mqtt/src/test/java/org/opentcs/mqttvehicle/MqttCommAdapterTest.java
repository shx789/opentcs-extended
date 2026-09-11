// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.mqttvehicle;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.ObjectMapper;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ScheduledThreadPoolExecutor;
import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.Test;
import org.opentcs.data.model.Location;
import org.opentcs.data.model.LocationType;
import org.opentcs.data.model.Path;
import org.opentcs.data.model.Point;
import org.opentcs.data.model.Vehicle;
import org.opentcs.data.order.DriveOrder;
import org.opentcs.data.order.Route;
import org.opentcs.data.order.TransportOrder;
import org.opentcs.drivers.vehicle.MovementCommand;
import org.opentcs.drivers.vehicle.VehicleCommAdapterMessage;

class MqttCommAdapterTest {

  private final ScheduledThreadPoolExecutor executor = new ScheduledThreadPoolExecutor(1);

  @AfterEach
  void tearDown() {
    executor.shutdownNow();
  }

  @Test
  void completesCurrentCommandOnSuccessFeedback() {
    Point sourcePoint = new Point("ST_IN_01");
    Point destinationPoint = new Point("P_WAIT_IN_01");
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, sourcePoint.getName())
        .withProperty(MqttCommAdapter.PROP_POINT_ID_MAP, "nav:1=P_WAIT_IN_01");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    MovementCommand command = command(sourcePoint, destinationPoint);
    adapter.getSentCommands().add(command);

    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"nav\",\"id\":1,\"status\":\"success\"}"
    );

    assertThat(adapter.getProcessModel().getPosition()).isEqualTo(destinationPoint.getName());
    assertThat(adapter.getSentCommands()).isEmpty();
  }

  @Test
  void completesNavigationStepEvenWhenDriveOrderOperationIsDrop() {
    Point sourcePoint = new Point("P_WAIT_IN_01");
    Point destinationPoint = new Point("ST_IN_01");
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, sourcePoint.getName())
        .withProperty(MqttCommAdapter.PROP_POINT_ID_MAP, "nav:0=ST_IN_01");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    MovementCommand command = command(
        sourcePoint,
        destinationPoint,
        destinationPoint,
        true,
        "DROP"
    );
    adapter.getSentCommands().add(command);

    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"nav\","
            + "\"id\":0,\"status\":\"success\"}"
    );

    assertThat(adapter.getProcessModel().getPosition()).isEqualTo(sourcePoint.getName());
    // Navigation success starts DROP; completion waits for lift feedback.
    assertThat(adapter.getSentCommands()).contains(command);
  }

  @Test
  void completesNavigationStepEvenWhenDriveOrderOperationIsPick() {
    Point sourcePoint = new Point("ST_IN_01");
    Point destinationPoint = new Point("P_WAIT_IN_01");
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, sourcePoint.getName())
        .withProperty(MqttCommAdapter.PROP_POINT_ID_MAP, "nav:1=P_WAIT_IN_01");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    MovementCommand command = command(
        sourcePoint,
        destinationPoint,
        destinationPoint,
        true,
        "PICK"
    );
    adapter.getSentCommands().add(command);

    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"nav\","
            + "\"id\":1,\"status\":\"success\"}"
    );

    assertThat(adapter.getProcessModel().getPosition()).isEqualTo(sourcePoint.getName());
    assertThat(adapter.getSentCommands()).contains(command);
  }

  @Test
  void ignoresSuccessFeedbackForAnotherPoint() {
    Point sourcePoint = new Point("ST_IN_01");
    Point destinationPoint = new Point("P_WAIT_IN_01");
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, sourcePoint.getName())
        .withProperty(MqttCommAdapter.PROP_POINT_ID_MAP, "nav:2=P_WAIT_IN_01");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    MovementCommand command = command(sourcePoint, destinationPoint);
    adapter.getSentCommands().add(command);

    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"nav\",\"id\":1,\"status\":\"success\"}"
    );

    assertThat(adapter.getProcessModel().getPosition()).isEqualTo(sourcePoint.getName());
    assertThat(adapter.getSentCommands()).containsExactly(command);
  }

  @Test
  void completesCurrentStepWhenFinalDestinationFeedbackArrivesEarly() {
    Point sourcePoint = new Point("ST_OUT_01");
    Point stepDestinationPoint = new Point("AGV_E_11");
    Point finalDestinationPoint = new Point("P_WAIT_IN_01");
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, sourcePoint.getName())
        .withProperty(MqttCommAdapter.PROP_POINT_ID_MAP, "nav:1=P_WAIT_IN_01");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    MovementCommand command = command(
        sourcePoint,
        stepDestinationPoint,
        finalDestinationPoint,
        false
    );
    adapter.getSentCommands().add(command);

    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"nav\",\"id\":1,\"status\":\"success\"}"
    );

    assertThat(adapter.getProcessModel().getPosition()).isEqualTo(stepDestinationPoint.getName());
    assertThat(adapter.getSentCommands()).isEmpty();
  }

  @Test
  void updatesPositionOnSuccessFeedbackWithoutCurrentCommand() {
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, "ST_IN_01")
        .withProperty(MqttCommAdapter.PROP_POINT_ID_MAP, "nav:4=AGV_E_04");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"nav\",\"id\":4,\"status\":\"success\"}"
    );

    assertThat(adapter.getProcessModel().getPosition()).isEqualTo("AGV_E_04");
    assertThat(adapter.getProcessModel().getState()).isEqualTo(Vehicle.State.IDLE);
    assertThat(adapter.getSentCommands()).isEmpty();
  }

  @Test
  void ignoresNonTaskFeedbackMessages() {
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, "ST_IN_01")
        .withProperty(MqttCommAdapter.PROP_POINT_ID_MAP, "nav:4=AGV_E_04");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"interest_point_control\",\"type\":\"nav\",\"id\":4,\"status\":\"success\"}"
    );

    assertThat(adapter.getProcessModel().getPosition()).isEqualTo("ST_IN_01");
  }

  @Test
  void acceptsManualPositionMessage() {
    Vehicle vehicle = new Vehicle("Vehicle-01");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    adapter.processMessage(
        new VehicleCommAdapterMessage(
            "tcs:virtualVehicle:setPosition",
            Map.of("position", "ST_IN_01")
        )
    );

    assertThat(adapter.getProcessModel().getPosition()).isEqualTo("ST_IN_01");
    assertThat(adapter.getProcessModel().getState()).isEqualTo(Vehicle.State.IDLE);
  }

  @Test
  void acceptsManualResetPositionMessage() {
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, "ST_IN_01");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    adapter.processMessage(
        new VehicleCommAdapterMessage(
            "tcs:virtualVehicle:resetPosition",
            Map.of()
        )
    );

    assertThat(adapter.getProcessModel().getPosition()).isNull();
  }

  @Test
  void buildsMagneticNavPayloadForLiftUpOperations() {
    MqttCommAdapter adapter = new MqttCommAdapter(new Vehicle("Vehicle-01"), executor);
    adapter.initialize();

    assertThat(adapter.buildLiftCommandPayload("PICK"))
        .containsEntry("cmd_type", "magnetic_nav")
        .containsEntry("aim_id", 0)
        .containsEntry("aim_dir", 0)
        .containsEntry("aim_action", 3);
    assertThat(adapter.buildLiftCommandPayload("LIFT_UP"))
        .containsEntry("aim_action", 3);
  }

  @Test
  void buildsMagneticNavPayloadForLiftDownOperations() {
    MqttCommAdapter adapter = new MqttCommAdapter(new Vehicle("Vehicle-01"), executor);
    adapter.initialize();

    assertThat(adapter.buildLiftCommandPayload("DROP"))
        .containsEntry("cmd_type", "magnetic_nav")
        .containsEntry("aim_id", 0)
        .containsEntry("aim_dir", 0)
        .containsEntry("aim_action", 4);
    assertThat(adapter.buildLiftCommandPayload("LIFT_DOWN"))
        .containsEntry("aim_action", 4);
  }

  @Test
  void buildsChargePointControlPayload() {
    Vehicle vehicle = new Vehicle("Vehicle-01");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    assertThat(adapter.buildChargePointControlPayload("goto", false, 0.8, 2))
        .containsEntry("cmd_type", "charge_point_control")
        .containsEntry("cmd", "goto")
        .containsEntry("path_mode", 0)
        .containsEntry("time", 2);
  }

  @Test
  void buildsChargePointAddPayload() {
    Vehicle vehicle = new Vehicle("Vehicle-01");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    assertThat(adapter.buildChargePointAddPayload(2.94, -0.83, -2.99))
        .containsEntry("cmd_type", "charge_point_add")
        .containsKey("point");
  }

  @Test
  void doesNotCompleteDropWhenForksStillRaised() {
    Point sourcePoint = new Point("P_WAIT_IN_01");
    Point destinationPoint = new Point("ST_IN_01");
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, sourcePoint.getName())
        .withProperty(MqttCommAdapter.PROP_POINT_ID_MAP, "nav:0=ST_IN_01")
        .withProperty(MqttCommAdapter.PROP_LIFT_SETTLE_TIME, "0");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    MovementCommand command = command(
        sourcePoint,
        destinationPoint,
        destinationPoint,
        true,
        "DROP"
    );
    adapter.getSentCommands().add(command);

    // Navigation success starts the DROP lift action.
    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"nav\",\"id\":0,\"status\":\"success\"}"
    );
    // AGV confirms the magnetic_nav action...
    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"magnetic_nav\",\"status\":\"success\"}"
    );
    // ...but the forks are still raised (low=false), even though no load remains.
    adapter.handleFeedback(
        "base_status",
        "{\"cmd_type\":\"base_status\","
            + "\"magnetic\":{\"material\":false,\"up\":true,\"low\":false}}"
    );

    assertThat(adapter.getSentCommands()).contains(command);
  }

  @Test
  void completesDropWhenLoadClearedAndForksLowered()
      throws Exception {
    Point sourcePoint = new Point("P_WAIT_IN_01");
    Point destinationPoint = new Point("ST_IN_01");
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, sourcePoint.getName())
        .withProperty(MqttCommAdapter.PROP_POINT_ID_MAP, "nav:0=ST_IN_01")
        .withProperty(MqttCommAdapter.PROP_LIFT_SETTLE_TIME, "0");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    MovementCommand command = command(
        sourcePoint,
        destinationPoint,
        destinationPoint,
        true,
        "DROP"
    );
    adapter.getSentCommands().add(command);

    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"nav\",\"id\":0,\"status\":\"success\"}"
    );
    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"magnetic_nav\",\"status\":\"success\"}"
    );
    adapter.handleFeedback(
        "base_status",
        "{\"cmd_type\":\"base_status\","
            + "\"magnetic\":{\"material\":false,\"up\":false,\"low\":true}}"
    );

    Thread.sleep(500);
    assertThat(adapter.getSentCommands()).isEmpty();
    assertThat(adapter.getProcessModel().getPosition()).isEqualTo(destinationPoint.getName());
    assertThat(adapter.getProcessModel().getLoadHandlingDevices()).hasSize(1);
    assertThat(adapter.getProcessModel().getLoadHandlingDevices().get(0).isFull()).isFalse();
  }

  @Test
  void failsPendingPickImmediatelyWhenAgvReportsNavErrorMode() {
    Point sourcePoint = new Point("P_WAIT_IN_01");
    Point destinationPoint = new Point("ST_IN_01");
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, sourcePoint.getName())
        .withProperty(MqttCommAdapter.PROP_POINT_ID_MAP, "nav:0=ST_IN_01")
        .withProperty(MqttCommAdapter.PROP_LIFT_SETTLE_TIME, "0");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    MovementCommand command = command(
        sourcePoint,
        destinationPoint,
        destinationPoint,
        true,
        "PICK"
    );
    adapter.getSentCommands().add(command);

    // Navigation success starts the PICK lift action.
    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"nav\",\"id\":0,\"status\":\"success\"}"
    );
    assertThat(adapter.getProcessModel().getState()).isEqualTo(Vehicle.State.EXECUTING);

    // The AGV reports navigation error mode (base_status status=9). The PICK
    // must fail immediately instead of waiting for the lift timeout.
    adapter.handleFeedback(
        "base_status",
        "{\"cmd_type\":\"base_status\",\"status\":9,"
            + "\"robot\":{\"mainerror\":3,\"suberror\":2,\"robot_status\":8},"
            + "\"magnetic\":{\"material\":false,\"up\":true,\"low\":false}}"
    );

    assertThat(adapter.getProcessModel().getState()).isEqualTo(Vehicle.State.IDLE);
  }

  @Test
  void doesNotFailPendingPickWhenAgvReportsNonErrorNavMode() {
    Point sourcePoint = new Point("P_WAIT_IN_01");
    Point destinationPoint = new Point("ST_IN_01");
    Vehicle vehicle = new Vehicle("Vehicle-01")
        .withProperty(MqttCommAdapter.PROP_INITIAL_POSITION, sourcePoint.getName())
        .withProperty(MqttCommAdapter.PROP_POINT_ID_MAP, "nav:0=ST_IN_01")
        .withProperty(MqttCommAdapter.PROP_LIFT_SETTLE_TIME, "0");
    MqttCommAdapter adapter = new MqttCommAdapter(vehicle, executor);
    adapter.initialize();

    MovementCommand command = command(
        sourcePoint,
        destinationPoint,
        destinationPoint,
        true,
        "PICK"
    );
    adapter.getSentCommands().add(command);

    adapter.handleFeedback(
        "task_feedback",
        "{\"cmd_type\":\"task_feedback\",\"type\":\"nav\",\"id\":0,\"status\":\"success\"}"
    );

    // status=8 (multi-task paused) with non-zero robot error codes is a normal
    // operating state: the PICK must remain in flight.
    adapter.handleFeedback(
        "base_status",
        "{\"cmd_type\":\"base_status\",\"status\":8,"
            + "\"robot\":{\"mainerror\":3,\"suberror\":2,\"robot_status\":8},"
            + "\"magnetic\":{\"material\":false,\"up\":true,\"low\":false}}"
    );

    assertThat(adapter.getProcessModel().getState()).isEqualTo(Vehicle.State.EXECUTING);
    assertThat(adapter.getSentCommands()).contains(command);
  }

  @Test
  void parsesBaseStatusNavMode()
      throws Exception {
    ObjectMapper mapper = new ObjectMapper();

    MqttCommAdapter.FeedbackMessage errorFeedback = MqttCommAdapter.FeedbackMessage.parse(
        mapper.readTree("{\"cmd_type\":\"base_status\",\"status\":9}")
    );
    assertThat(errorFeedback.navStatus()).isEqualTo(9);
    assertThat(errorFeedback.isNavError()).isTrue();

    MqttCommAdapter.FeedbackMessage pausedFeedback = MqttCommAdapter.FeedbackMessage.parse(
        mapper.readTree("{\"cmd_type\":\"base_status\",\"status\":\"8\"}")
    );
    assertThat(pausedFeedback.navStatus()).isEqualTo(8);
    assertThat(pausedFeedback.isNavError()).isFalse();

    // task_feedback reuses the "status" field with word values, which must
    // not be interpreted as a navigation mode.
    MqttCommAdapter.FeedbackMessage taskFeedback = MqttCommAdapter.FeedbackMessage.parse(
        mapper.readTree(
            "{\"cmd_type\":\"task_feedback\",\"type\":\"nav\",\"id\":0,\"status\":\"success\"}"
        )
    );
    assertThat(taskFeedback.navStatus()).isNull();
    assertThat(taskFeedback.isNavError()).isFalse();
  }

  @Test
  void parsesBaseStatusMagneticFeedback()
      throws Exception {
    String payload = """
        {
          "bms": {"soc": 42},
          "cmd_type": "base_status",
          "magnetic": {"low": true, "material": false, "up": false},
          "robot": {"mainerror": 3, "suberror": 12, "robot_status": 8}
        }
        """;

    MqttCommAdapter.FeedbackMessage feedback = MqttCommAdapter.FeedbackMessage.parse(
        new com.fasterxml.jackson.databind.ObjectMapper().readTree(payload)
    );

    assertThat(feedback.isSupportedFeedback()).isTrue();
    assertThat(feedback.energyLevel()).isEqualTo(42);
    assertThat(feedback.material()).isFalse();
    assertThat(feedback.up()).isFalse();
    assertThat(feedback.down()).isTrue();
    assertThat(feedback.mainError()).isEqualTo(3);
    assertThat(feedback.subError()).isEqualTo(12);
    assertThat(feedback.robotStatus()).isEqualTo(8);
  }

  @Test
  void ignoresAllZeroBmsPlaceholder()
      throws Exception {
    String payload = """
        {
          "cmd_type": "base_status",
          "bms": {
            "remaining_capacity": 0,
            "soc": 0,
            "status": 0,
            "voltage": 0.0
          }
        }
        """;

    MqttCommAdapter.FeedbackMessage feedback = MqttCommAdapter.FeedbackMessage.parse(
        new com.fasterxml.jackson.databind.ObjectMapper().readTree(payload)
    );

    assertThat(feedback.energyLevel()).isNull();
  }

  private MovementCommand command(Point sourcePoint, Point destinationPoint) {
    return command(
        sourcePoint, destinationPoint, destinationPoint, true, MovementCommand.MOVE_OPERATION
    );
  }

  private MovementCommand command(
      Point sourcePoint,
      Point destinationPoint,
      Point finalDestinationPoint,
      boolean finalMovement
  ) {
    return command(
        sourcePoint,
        destinationPoint,
        finalDestinationPoint,
        finalMovement,
        MovementCommand.MOVE_OPERATION
    );
  }

  private MovementCommand command(
      Point sourcePoint,
      Point destinationPoint,
      Point finalDestinationPoint,
      boolean finalMovement,
      String operation
  ) {
    Path path = new Path(
        sourcePoint.getName() + " --- " + destinationPoint.getName(),
        sourcePoint.getReference(),
        destinationPoint.getReference()
    );
    Route.Step step = new Route.Step(
        path,
        sourcePoint,
        destinationPoint,
        Vehicle.Orientation.FORWARD,
        0,
        1
    );
    DriveOrder driveOrder = new DriveOrder(
        "DriveOrder-01",
        new DriveOrder.Destination(destinationPoint.getReference())
    );
    TransportOrder transportOrder = new TransportOrder("T-01", List.of(driveOrder));
    DriveOrder driveOrderWithTransportOrder = transportOrder.getAllDriveOrders().getFirst();
    LocationType locationType = new LocationType("LocationType-01");
    Location operationLocation = new Location("Location-01", locationType.getReference());

    return new MovementCommand(
        transportOrder,
        driveOrderWithTransportOrder,
        step,
        operation,
        operation.equals(MovementCommand.MOVE_OPERATION) ? null : operationLocation,
        finalMovement,
        null,
        finalDestinationPoint,
        operation,
        Map.of()
    );
  }
}
