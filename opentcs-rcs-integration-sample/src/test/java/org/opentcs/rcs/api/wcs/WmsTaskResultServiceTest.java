// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.time.Instant;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.callback.CallbackOutboxService;
import org.opentcs.rcs.callback.InMemoryCallbackOutboxStore;
import org.opentcs.rcs.core.task.WcsTaskRecord;
import org.opentcs.rcs.core.task.WcsTaskStatus;
import org.opentcs.rcs.core.task.WcsTaskType;
import org.opentcs.rcs.http.RequestContext;

class WmsTaskResultServiceTest {

  @Test
  void shouldEnqueueOutboundDoneResultWhenConfigured()
      throws Exception {
    InMemoryCallbackOutboxStore outboxStore = new InMemoryCallbackOutboxStore();
    ObjectMapper objectMapper = new ObjectMapper();
    WmsTaskResultService service = new WmsTaskResultService(
        new CallbackOutboxService(outboxStore, objectMapper),
        "http://127.0.0.1:18081/"
    );

    service.enqueueResultIfConfigured(
        taskRecord(),
        WcsTaskStatus.DONE,
        new RequestContext("trace-1", "request-1"),
        null,
        null,
        "2026-04-22T08:08:08Z"
    );

    var entries = outboxStore.findDue(Instant.now().plusSeconds(1), 10);
    assertThat(entries).hasSize(1);
    assertThat(entries.get(0).callbackUrl()).isEqualTo(
        "http://127.0.0.1:18081/api/v1/wms/outbound-results"
    );
    assertThat(entries.get(0).idemKey()).isEqualTo("BIZ-OUT-001|DONE");
    JsonNode payload = objectMapper.readTree(entries.get(0).payloadJson());
    assertThat(payload.get("biz_task_no").asText()).isEqualTo("BIZ-OUT-001");
    assertThat(payload.get("result_type").asText()).isEqualTo("DONE");
    assertThat(payload.get("task_type").asText()).isEqualTo("OUTBOUND");
  }

  @Test
  void shouldSkipWhenBaseUrlNotConfigured() {
    InMemoryCallbackOutboxStore outboxStore = new InMemoryCallbackOutboxStore();
    WmsTaskResultService service = new WmsTaskResultService(
        new CallbackOutboxService(outboxStore, new ObjectMapper()),
        null
    );

    service.enqueueResultIfConfigured(
        taskRecord(),
        WcsTaskStatus.DONE,
        new RequestContext("trace-1", "request-1"),
        null,
        null,
        null
    );

    assertThat(outboxStore.findDue(Instant.now().plusSeconds(1), 10)).isEmpty();
  }

  private WcsTaskRecord taskRecord() {
    return new WcsTaskRecord(
        "BIZ-OUT-001",
        WcsTaskType.OUTBOUND,
        "M_OUT_001",
        "T_OUT_001",
        null,
        WcsTaskStatus.RECEIVED,
        "Point-0026",
        "Point-0020",
        "PLT-1",
        50,
        "/api/v1/wcs/agv/events",
        "trace-0",
        "request-0",
        "2026-04-22T08:00:00Z",
        "2026-04-22T08:00:00Z"
    );
  }
}
