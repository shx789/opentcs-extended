// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.core.task;

import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;

/**
 * In-memory task store for local runs/tests.
 */
public class InMemoryTaskStore
    implements
      TaskStore {

  private final ConcurrentMap<String, WcsTaskRecord> recordsByBizTaskNo = new ConcurrentHashMap<>();
  private final ConcurrentMap<String, String> bizTaskNoByMissionNo = new ConcurrentHashMap<>();

  @Override
  public synchronized void save(WcsTaskRecord record) {
    WcsTaskRecord oldRecord = recordsByBizTaskNo.put(record.bizTaskNo(), record);
    if (oldRecord != null) {
      bizTaskNoByMissionNo.remove(oldRecord.missionNo());
    }
    bizTaskNoByMissionNo.put(record.missionNo(), record.bizTaskNo());
  }

  @Override
  public Optional<WcsTaskRecord> findByBizTaskNo(String bizTaskNo) {
    return Optional.ofNullable(recordsByBizTaskNo.get(bizTaskNo));
  }

  @Override
  public Optional<WcsTaskRecord> findByMissionNo(String missionNo) {
    return Optional.ofNullable(bizTaskNoByMissionNo.get(missionNo))
        .flatMap(this::findByBizTaskNo);
  }
}
