// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.task;

import java.util.Optional;

/**
 * Storage abstraction for WMS/WCS task aggregates.
 */
public interface TaskStore {

  void save(WcsTaskRecord record);

  Optional<WcsTaskRecord> findByBizTaskNo(String bizTaskNo);

  Optional<WcsTaskRecord> findByMissionNo(String missionNo);
}
