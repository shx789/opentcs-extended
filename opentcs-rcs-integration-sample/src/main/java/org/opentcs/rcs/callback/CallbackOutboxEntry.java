// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

import java.time.Instant;

/**
 * Outbox callback message.
 */
public class CallbackOutboxEntry {

  private final String missionNo;
  private final String callbackUrl;
  private final String payloadJson;
  private final String idemKey;
  private String status;
  private int retryCount;
  private Instant nextRetryAt;
  private String lastError;

  public CallbackOutboxEntry(
      String missionNo,
      String callbackUrl,
      String payloadJson,
      String idemKey,
      String status,
      int retryCount,
      Instant nextRetryAt,
      String lastError
  ) {
    this.missionNo = missionNo;
    this.callbackUrl = callbackUrl;
    this.payloadJson = payloadJson;
    this.idemKey = idemKey;
    this.status = status;
    this.retryCount = retryCount;
    this.nextRetryAt = nextRetryAt;
    this.lastError = lastError;
  }

  public String missionNo() {
    return missionNo;
  }

  public String callbackUrl() {
    return callbackUrl;
  }

  public String payloadJson() {
    return payloadJson;
  }

  public String idemKey() {
    return idemKey;
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
