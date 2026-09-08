// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.agvcommand;

import java.time.Instant;

/**
 * Outbox entry for AGV MQTT control commands.
 */
public class AgvCommandOutboxEntry {

  private final String missionNo;
  private final String taskNo;
  private final String commandStage;
  private final String idemKey;
  private final String payloadJson;
  private String status;
  private int retryCount;
  private Instant nextRetryAt;
  private String lastError;

  public AgvCommandOutboxEntry(
      String missionNo,
      String taskNo,
      String commandStage,
      String idemKey,
      String payloadJson,
      String status,
      int retryCount,
      Instant nextRetryAt,
      String lastError
  ) {
    this.missionNo = missionNo;
    this.taskNo = taskNo;
    this.commandStage = commandStage;
    this.idemKey = idemKey;
    this.payloadJson = payloadJson;
    this.status = status;
    this.retryCount = retryCount;
    this.nextRetryAt = nextRetryAt;
    this.lastError = lastError;
  }

  public String missionNo() {
    return missionNo;
  }

  public String taskNo() {
    return taskNo;
  }

  public String commandStage() {
    return commandStage;
  }

  public String idemKey() {
    return idemKey;
  }

  public String payloadJson() {
    return payloadJson;
  }

  public String status() {
    return status;
  }

  public void setStatus(String status) {
    this.status = status;
  }

  public int retryCount() {
    return retryCount;
  }

  public void setRetryCount(int retryCount) {
    this.retryCount = retryCount;
  }

  public Instant nextRetryAt() {
    return nextRetryAt;
  }

  public void setNextRetryAt(Instant nextRetryAt) {
    this.nextRetryAt = nextRetryAt;
  }

  public String lastError() {
    return lastError;
  }

  public void setLastError(String lastError) {
    this.lastError = lastError;
  }
}
