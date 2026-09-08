// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import io.javalin.http.Context;
import io.javalin.http.Handler;
import java.util.Objects;
import java.util.Optional;
import org.opentcs.rcs.api.dto.AgvEventCallbackReq;
import org.opentcs.rcs.callback.CallbackRetryProcessor;
import org.opentcs.rcs.http.RequestContext;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * HTTP handlers for ingesting openTCS transport-order events.
 */
public final class OpenTcsEventHttpHandlers {

  private static final Logger LOG = LoggerFactory.getLogger(OpenTcsEventHttpHandlers.class);

  private OpenTcsEventHttpHandlers() {
  }

  public static Handler transportOrderEventHandler(
      OpenTcsSsePayloadParser payloadParser,
      OpenTcsSseEventConsumer eventConsumer,
      CallbackRetryProcessor retryProcessor,
      int callbackDispatchBatchSize
  ) {
    Objects.requireNonNull(payloadParser, "payloadParser");
    Objects.requireNonNull(eventConsumer, "eventConsumer");
    Objects.requireNonNull(retryProcessor, "retryProcessor");
    if (callbackDispatchBatchSize < 1) {
      throw new IllegalArgumentException("callbackDispatchBatchSize must be greater than 0");
    }
    return ctx -> handleTransportOrderEvent(
        ctx,
        payloadParser,
        eventConsumer,
        retryProcessor,
        callbackDispatchBatchSize
    );
  }

  private static void handleTransportOrderEvent(
      Context ctx,
      OpenTcsSsePayloadParser payloadParser,
      OpenTcsSseEventConsumer eventConsumer,
      CallbackRetryProcessor retryProcessor,
      int callbackDispatchBatchSize
  ) {
    RequestContext requestContext = RequestContext.from(ctx);
    requestContext.writeToResponse(ctx);
    OpenTcsTransportOrderEvent event = payloadParser.parseTransportOrderEvent(ctx.body());
    Optional<AgvEventCallbackReq> mapped = eventConsumer.consume(event, requestContext);
    LOG.info(
        "openTCS event accepted. missionNo={}, mapped={}, traceId={}, requestId={}",
        event.missionNo(),
        mapped.isPresent(),
        requestContext.traceId(),
        requestContext.requestId()
    );
    retryProcessor.processDue(callbackDispatchBatchSize);
    ctx.status(202).json(
        new EventAcceptedResp(
            true,
            mapped.isPresent(),
            event.missionNo()
        )
    );
  }

  private record EventAcceptedResp(
      boolean accepted,
      boolean mapped,
      String missionNo
  ) {
  }
}
