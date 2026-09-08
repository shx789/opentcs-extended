// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import static org.assertj.core.api.Assertions.assertThat;

import java.time.Instant;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.api.dto.AgvEventCallbackReq;
import org.opentcs.rcs.http.RequestContext;

class OpenTcsEventProjectorTest {

  private static final RequestContext REQUEST_CONTEXT = new RequestContext("trace-1", "request-1");

  @Test
  void shouldMapBeingProcessedIndexZeroToArrivedFrom() {
    OpenTcsEventProjector projector = new OpenTcsEventProjector();
    OpenTcsTransportOrderEvent event = new OpenTcsTransportOrderEvent(
        "M1",
        "T1",
        "BEING_PROCESSED",
        0,
        "Point-0001",
        "AGV_01",
        null,
        Instant.parse("2026-02-10T10:35:21Z")
    );

    AgvEventCallbackReq callback = projector.toWcsEvent(event, REQUEST_CONTEXT).orElseThrow();
    assertThat(callback.eventType()).isEqualTo("ARRIVED_FROM");
    assertThat(callback.pointId()).isEqualTo("Point-0001");
    assertThat(callback.agvId()).isEqualTo("AGV_01");
    assertThat(callback.traceId()).isEqualTo("trace-1");
    assertThat(callback.requestId()).isEqualTo("request-1");
    assertThat(callback.eventTime()).isEqualTo("2026-02-10 10:35:21");
  }

  @Test
  void shouldMapFailedToFailedWithReason() {
    OpenTcsEventProjector projector = new OpenTcsEventProjector();
    OpenTcsTransportOrderEvent event = new OpenTcsTransportOrderEvent(
        "M2",
        "T2",
        "FAILED",
        1,
        "Point-0020",
        "AGV_02",
        "battery below threshold",
        Instant.parse("2026-02-10T10:35:21Z")
    );

    AgvEventCallbackReq callback = projector.toWcsEvent(event, REQUEST_CONTEXT).orElseThrow();
    assertThat(callback.eventType()).isEqualTo("FAILED");
    assertThat(callback.pointId()).isEqualTo("Point-0020");
    assertThat(callback.reasonCode()).isEqualTo("RCS_FAILED");
    assertThat(callback.reasonMsg()).isEqualTo("battery below threshold");
  }
}
