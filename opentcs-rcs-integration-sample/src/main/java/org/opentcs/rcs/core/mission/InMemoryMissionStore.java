// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.mission;

import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

/**
 * In-memory mission store for local runs/tests.
 */
public class InMemoryMissionStore
    implements MissionStore {

  private final ConcurrentMap<String, MissionCallbackTarget> targets = new ConcurrentHashMap<>();

  @Override
  public void save(MissionCallbackTarget target) {
    targets.put(target.missionNo(), target);
  }

  @Override
  public Optional<MissionCallbackTarget> findByMissionNo(String missionNo) {
    return Optional.ofNullable(targets.get(missionNo));
  }
}
