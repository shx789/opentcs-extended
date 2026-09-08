// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.agvcommand;

/**
 * Sends serialized AGV control commands to the field adapter.
 */
public interface AgvCommandSender {

  void send(String payloadJson);
}
