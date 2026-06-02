// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.agvcommand;

import java.time.Instant;
import java.util.Objects;
import org.opentcs.rcs.api.dto.Mission;
import org.opentcs.rcs.bridge.agv.AgvCommandPublisher;
import org.opentcs.rcs.bridge.agv.MqttAgvRobotControlPublisher;

/**
 * Converts missions into idempotent AGV command outbox entries.
 */
public class AgvCommandOutboxService
    implements AgvCommandPublisher {

  public static final String STAGE_MISSION_START = "MISSION_START";

  private final AgvCommandOutboxStore store;
  private final MqttAgvRobotControlPublisher commandPublisher;

  public AgvCommandOutboxService(
      AgvCommandOutboxStore store,
      MqttAgvRobotControlPublisher commandPublisher
  ) {
    this.store = Objects.requireNonNull(store, "store");
    this.commandPublisher = Objects.requireNonNull(commandPublisher, "commandPublisher");
  }

  @Override
  public void publishMissionStart(Mission mission) {
    Objects.requireNonNull(mission, "mission");
    String idemKey = idemKey(mission.missionNo(), STAGE_MISSION_START);
    if (store.findByIdemKey(idemKey).isPresent()) {
      return;
    }
    store.save(
        new AgvCommandOutboxEntry(
            mission.missionNo(),
            mission.taskNo(),
            STAGE_MISSION_START,
            idemKey,
            commandPublisher.buildMissionStartPayloadJson(mission),
            "PENDING",
            0,
            Instant.now(),
            null
        )
    );
  }

  public static String idemKey(String missionNo, String commandStage) {
    return missionNo + ":" + commandStage;
  }
}
