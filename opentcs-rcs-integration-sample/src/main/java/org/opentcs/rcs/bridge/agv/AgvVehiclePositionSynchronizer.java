// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv;

import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import org.opentcs.rcs.bridge.agv.mapping.AgvPointMapping;
import org.opentcs.rcs.bridge.agv.mapping.AgvPointMappingStore;
import org.opentcs.rcs.bridge.opentcs.OpenTcsVehiclePositionClient;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Synchronizes AGV arrival feedback to openTCS vehicle positions.
 */
public class AgvVehiclePositionSynchronizer {

  private static final Logger LOG = LoggerFactory.getLogger(AgvVehiclePositionSynchronizer.class);

  private final OpenTcsVehiclePositionClient positionClient;
  private final AgvPointMappingStore pointMappingStore;
  private final Map<String, String> vehicleNamesByAgvId;

  public AgvVehiclePositionSynchronizer(
      OpenTcsVehiclePositionClient positionClient,
      AgvPointMappingStore pointMappingStore,
      Map<String, String> vehicleNamesByAgvId
  ) {
    this.positionClient = Objects.requireNonNull(positionClient, "positionClient");
    this.pointMappingStore = Objects.requireNonNull(pointMappingStore, "pointMappingStore");
    this.vehicleNamesByAgvId = Map.copyOf(
        Objects.requireNonNull(vehicleNamesByAgvId, "vehicleNamesByAgvId")
    );
  }

  public void synchronize(AgvMqttStatusMessage message) {
    Objects.requireNonNull(message, "message");
    if (!shouldSynchronize(message.eventType())) {
      return;
    }
    String vehicleName = vehicleNameFor(message.agvId());
    if (vehicleName == null) {
      LOG.debug("No openTCS vehicle mapping configured for AGV id {}", message.agvId());
      return;
    }
    Optional<AgvPointMapping> mapping = pointMappingStore.findByName(message.pointId())
        .or(() -> pointMappingStore.findNearest(message.rosX(), message.rosY()));
    if (mapping.isEmpty()) {
      LOG.debug(
          "No openTCS point mapping found for AGV feedback: agvId={} pointId={} x={} y={}",
          message.agvId(),
          message.pointId(),
          message.rosX(),
          message.rosY()
      );
      return;
    }
    positionClient.updateVehiclePosition(vehicleName, mapping.orElseThrow().pointName());
    LOG.info(
        "Synchronized AGV position to openTCS: agvId={} vehicle={} point={}",
        message.agvId(),
        vehicleName,
        mapping.orElseThrow().pointName()
    );
  }

  private String vehicleNameFor(String agvId) {
    if (agvId == null || agvId.isBlank()) {
      return null;
    }
    return vehicleNamesByAgvId.getOrDefault(agvId, vehicleNamesByAgvId.get("*"));
  }

  private boolean shouldSynchronize(String eventType) {
    if (eventType == null) {
      return false;
    }
    return switch (eventType) {
      case "ARRIVED", "ARRIVED_FROM", "ARRIVED_TO", "COMPLETED", "COMPLETE", "FINISHED" -> true;
      default -> false;
    };
  }
}
