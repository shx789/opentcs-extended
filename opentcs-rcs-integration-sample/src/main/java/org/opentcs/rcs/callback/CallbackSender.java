// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

/**
 * Sends callback payload to WCS endpoint.
 */
public interface CallbackSender {

  void send(String callbackUrl, String payloadJson) throws Exception;
}
