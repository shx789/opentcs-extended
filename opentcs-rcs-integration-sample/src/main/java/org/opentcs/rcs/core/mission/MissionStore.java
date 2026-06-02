// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.mission;

import java.util.List;
import java.util.Optional;

/**
 * Mission storage abstraction for callback target resolution.
 */
public interface MissionStore {

  void save(MissionCallbackTarget target);

  Optional<MissionCallbackTarget> findByMissionNo(String missionNo);

  List<MissionCallbackTarget> findAll();

  default void updateStatus(String missionNo, String rcsStatus) {
    findByMissionNo(missionNo).ifPresent(target -> save(target.withRcsStatus(rcsStatus)));
  }
}
