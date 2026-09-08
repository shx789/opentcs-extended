// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.http;

import io.javalin.http.Context;
import java.util.Objects;
import java.util.UUID;

/**
 * Request-scoped tracing identifiers.
 */
public record RequestContext(
    String traceId,
    String requestId
) {

  public static final String TRACE_ID_HEADER = "X-Trace-Id";
  public static final String REQUEST_ID_HEADER = "X-Request-Id";

  public RequestContext {
    traceId = requireNonBlank(traceId, "traceId");
    requestId = requireNonBlank(requestId, "requestId");
  }

  public static RequestContext from(Context ctx) {
    Objects.requireNonNull(ctx, "ctx");
    return new RequestContext(
        resolveOrGenerate(ctx.header(TRACE_ID_HEADER)),
        resolveOrGenerate(ctx.header(REQUEST_ID_HEADER))
    );
  }

  public static RequestContext generated() {
    return new RequestContext(UUID.randomUUID().toString(), UUID.randomUUID().toString());
  }

  public void writeToResponse(Context ctx) {
    Objects.requireNonNull(ctx, "ctx");
    ctx.header(TRACE_ID_HEADER, traceId);
    ctx.header(REQUEST_ID_HEADER, requestId);
  }

  private static String resolveOrGenerate(String raw) {
    if (raw == null || raw.isBlank()) {
      return UUID.randomUUID().toString();
    }
    return raw.trim();
  }

  private static String requireNonBlank(String value, String fieldName) {
    Objects.requireNonNull(value, fieldName);
    if (value.isBlank()) {
      throw new IllegalArgumentException(fieldName + " must not be blank");
    }
    return value;
  }
}
