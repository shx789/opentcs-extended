// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.agvcommand;

import java.time.Instant;
import java.util.List;
import java.util.Optional;

/**
 * Storage for AGV command outbox entries.
 */
public interface AgvCommandOutboxStore {

  void save(AgvCommandOutboxEntry entry);

  Optional<AgvCommandOutboxEntry> findByIdemKey(String idemKey);

  List<AgvCommandOutboxEntry> findByMissionNo(String missionNo);

  List<AgvCommandOutboxEntry> findDue(Instant now, int limit);
}
