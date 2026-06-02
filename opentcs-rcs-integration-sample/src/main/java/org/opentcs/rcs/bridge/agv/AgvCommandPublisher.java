// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv;

import org.opentcs.rcs.api.dto.Mission;

/**
 * Publishes AGV control commands derived from WCS/RCS missions.
 */
public interface AgvCommandPublisher {

  void publishMissionStart(Mission mission);

  static AgvCommandPublisher noop() {
    return mission -> {
    };
  }
}
