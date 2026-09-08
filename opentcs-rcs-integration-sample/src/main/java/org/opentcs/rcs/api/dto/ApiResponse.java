// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.dto;

/**
 * Generic API response envelope.
 */
public record ApiResponse<T>(
    String code,
    String msg,
    T data
) {

  public static <T> ApiResponse<T> success(T data) {
    return new ApiResponse<>("0", "OK", data);
  }

  public static ApiResponse<Void> error(String code, String msg) {
    return new ApiResponse<>(code, msg, null);
  }
}
