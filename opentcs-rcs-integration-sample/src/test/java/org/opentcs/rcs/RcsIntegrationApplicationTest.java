// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs;

import static org.assertj.core.api.Assertions.assertThat;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.sun.net.httpserver.HttpServer;
import io.javalin.Javalin;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.file.Path;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;

class RcsIntegrationApplicationTest {

  @TempDir
  Path tempDir;

  @Test
  void shouldAcceptMissionCreationRequest() throws Exception {
    Javalin app = RcsIntegrationApplication.createApp();
    app.start(0);
    try {
      int port = app.port();
      String body = """
          {
            "mission_no": "M202602100001",
            "task_no": "T202602090001",
            "from_point": "P_WAIT_IN_01",
            "to_point": "ST_IN_01",
            "pallet_no": "PLT000000123",
            "priority": 30,
            "callback_url": "/api/v1/wcs/agv/events"
          }
          """;
      HttpResponse<String> response = postJson(port, "/api/v1/wcs/agv/missions", body);

      assertThat(response.statusCode()).isEqualTo(200);
      JsonNode json = new ObjectMapper().readTree(response.body());
      assertThat(json.get("code").asText()).isEqualTo("0");
      assertThat(json.get("data").get("mission_no").asText()).isEqualTo("M202602100001");
      assertThat(json.get("data").get("rcs_status").asText()).isEqualTo("RECEIVED");
      assertThat(json.get("data").get("idem_hit").asBoolean()).isFalse();
    }
    finally {
      app.stop();
    }
  }

  @Test
  void shouldDispatchMappedCallbackWhenOpenTcsEventIsPosted() throws Exception {
    AtomicInteger callbackCalls = new AtomicInteger();
    AtomicReference<String> callbackPayload = new AtomicReference<>();
    HttpServer callbackServer = HttpServer.create(new InetSocketAddress(0), 0);
    callbackServer.createContext(
        "/api/v1/wcs/agv/events",
        exchange -> {
          callbackCalls.incrementAndGet();
          callbackPayload.set(new String(exchange.getRequestBody().readAllBytes(), StandardCharsets.UTF_8));
          byte[] bytes = "{}".getBytes(StandardCharsets.UTF_8);
          exchange.sendResponseHeaders(200, bytes.length);
          exchange.getResponseBody().write(bytes);
          exchange.close();
        }
    );
    callbackServer.start();

    Javalin app = RcsIntegrationApplication.createApp();
    app.start(0);
    try {
      int appPort = app.port();
      int callbackPort = callbackServer.getAddress().getPort();
      String createMissionBody = """
          {
            "mission_no": "M202604140001",
            "task_no": "T202604140001",
            "from_point": "P_WAIT_IN_01",
            "to_point": "ST_IN_01",
            "pallet_no": "PLT000000123",
            "priority": 30,
            "callback_url": "http://127.0.0.1:%d/api/v1/wcs/agv/events"
          }
          """.formatted(callbackPort);
      HttpResponse<String> createMissionResp = postJson(
          appPort,
          "/api/v1/wcs/agv/missions",
          createMissionBody
      );
      assertThat(createMissionResp.statusCode()).isEqualTo(200);

      String openTcsEventBody = """
          {
            "eventTime": "2026-04-14T10:35:21Z",
            "currentObjectState": {
              "name": "M202604140001",
              "state": "FINISHED",
              "currentDriveOrderIndex": 1,
              "processingVehicle": "AGV_01",
              "properties": {
                "task_no": "T202604140001"
              }
            }
          }
          """;
      HttpResponse<String> eventResp = postJson(
          appPort,
          "/api/v1/opentcs/events/transport-orders",
          openTcsEventBody
      );

      assertThat(eventResp.statusCode()).isEqualTo(202);
      assertThat(callbackCalls.get()).isEqualTo(1);
      assertThat(callbackPayload.get()).contains("\"event_type\":\"DROPPED\"");
      assertThat(callbackPayload.get()).contains("\"mission_no\":\"M202604140001\"");
    }
    finally {
      app.stop();
      callbackServer.stop(0);
    }
  }

  @Test
  void shouldExposeCallbackOutboxRecords() throws Exception {
    HttpServer callbackServer = HttpServer.create(new InetSocketAddress(0), 0);
    callbackServer.createContext(
        "/api/v1/wcs/agv/events",
        exchange -> {
          byte[] bytes = "{}".getBytes(StandardCharsets.UTF_8);
          exchange.sendResponseHeaders(200, bytes.length);
          exchange.getResponseBody().write(bytes);
          exchange.close();
        }
    );
    callbackServer.start();

    Javalin app = RcsIntegrationApplication.createApp();
    app.start(0);
    try {
      int appPort = app.port();
      int callbackPort = callbackServer.getAddress().getPort();
      String callbackUrl = "http://127.0.0.1:%d/api/v1/wcs/agv/events".formatted(callbackPort);
      String createMissionBody = """
          {
            "mission_no": "M202604140099",
            "task_no": "T202604140099",
            "from_point": "P_WAIT_IN_01",
            "to_point": "ST_IN_01",
            "pallet_no": "PLT000000123",
            "priority": 30,
            "callback_url": "%s"
          }
          """.formatted(callbackUrl);
      HttpResponse<String> createMissionResp = postJson(
          appPort,
          "/api/v1/wcs/agv/missions",
          createMissionBody
      );
      assertThat(createMissionResp.statusCode()).isEqualTo(200);

      String openTcsEventBody = """
          {
            "eventTime": "2026-04-14T10:35:21Z",
            "currentObjectState": {
              "name": "M202604140099",
              "state": "FINISHED",
              "currentDriveOrderIndex": 1,
              "processingVehicle": "AGV_01",
              "properties": {
                "task_no": "T202604140099"
              }
            }
          }
          """;
      HttpResponse<String> eventResp = postJson(
          appPort,
          "/api/v1/opentcs/events/transport-orders",
          openTcsEventBody
      );
      assertThat(eventResp.statusCode()).isEqualTo(202);

      HttpResponse<String> callbacksResp = getJson(
          appPort,
          "/api/v1/rcs/callbacks?mission_no=M202604140099"
      );
      assertThat(callbacksResp.statusCode()).isEqualTo(200);
      JsonNode json = new ObjectMapper().readTree(callbacksResp.body());
      assertThat(json.get("code").asText()).isEqualTo("0");
      assertThat(json.get("data")).hasSize(1);
      JsonNode record = json.get("data").get(0);
      assertThat(record.get("mission_no").asText()).isEqualTo("M202604140099");
      assertThat(record.get("callback_url").asText()).isEqualTo(callbackUrl);
      assertThat(record.get("status").asText()).isEqualTo("SUCCESS");
      assertThat(record.get("retry_count").asInt()).isZero();
      assertThat(record.get("payload").get("event_type").asText()).isEqualTo("DROPPED");
    }
    finally {
      app.stop();
      callbackServer.stop(0);
    }
  }

  @Test
  void shouldKeepMissionCallbackTargetAfterRestartWhenFileStoreModeEnabled() throws Exception {
    AtomicInteger callbackCalls = new AtomicInteger();
    HttpServer callbackServer = HttpServer.create(new InetSocketAddress(0), 0);
    callbackServer.createContext(
        "/api/v1/wcs/agv/events",
        exchange -> {
          callbackCalls.incrementAndGet();
          byte[] bytes = "{}".getBytes(StandardCharsets.UTF_8);
          exchange.sendResponseHeaders(200, bytes.length);
          exchange.getResponseBody().write(bytes);
          exchange.close();
        }
    );
    callbackServer.start();

    Path storeDir = tempDir.resolve("file-store");
    System.setProperty("rcs.store.mode", "file");
    System.setProperty("rcs.store.file.dir", storeDir.toString());
    try {
      int callbackPort = callbackServer.getAddress().getPort();
      String createMissionBody = """
          {
            "mission_no": "M202604150002",
            "task_no": "T202604150002",
            "from_point": "P_WAIT_IN_01",
            "to_point": "ST_IN_01",
            "pallet_no": "PLT000000123",
            "priority": 30,
            "callback_url": "http://127.0.0.1:%d/api/v1/wcs/agv/events"
          }
          """.formatted(callbackPort);
      String openTcsEventBody = """
          {
            "eventTime": "2026-04-15T10:35:21Z",
            "currentObjectState": {
              "name": "M202604150002",
              "state": "FINISHED",
              "currentDriveOrderIndex": 1,
              "processingVehicle": "AGV_01",
              "properties": {
                "task_no": "T202604150002"
              }
            }
          }
          """;

      Javalin firstApp = RcsIntegrationApplication.createApp();
      firstApp.start(0);
      try {
        HttpResponse<String> createMissionResp = postJson(
            firstApp.port(),
            "/api/v1/wcs/agv/missions",
            createMissionBody
        );
        assertThat(createMissionResp.statusCode()).isEqualTo(200);
      }
      finally {
        firstApp.stop();
      }

      Javalin secondApp = RcsIntegrationApplication.createApp();
      secondApp.start(0);
      try {
        HttpResponse<String> eventResp = postJson(
            secondApp.port(),
            "/api/v1/opentcs/events/transport-orders",
            openTcsEventBody
        );
        assertThat(eventResp.statusCode()).isEqualTo(202);
        assertThat(callbackCalls.get()).isEqualTo(1);
      }
      finally {
        secondApp.stop();
      }
    }
    finally {
      System.clearProperty("rcs.store.mode");
      System.clearProperty("rcs.store.file.dir");
      callbackServer.stop(0);
    }
  }

  @Test
  void shouldQueryAndCancelMission() throws Exception {
    Javalin app = RcsIntegrationApplication.createApp();
    app.start(0);
    try {
      int port = app.port();
      String createBody = """
          {
            "mission_no": "M202604200001",
            "task_no": "T202604200001",
            "from_point": "P_WAIT_IN_01",
            "to_point": "ST_IN_01",
            "pallet_no": "PLT000000123",
            "priority": 30,
            "callback_url": "http://127.0.0.1:8081/api/v1/wcs/agv/events"
          }
          """;
      HttpResponse<String> createResp = postJson(port, "/api/v1/wcs/agv/missions", createBody);
      assertThat(createResp.statusCode()).isEqualTo(200);

      HttpResponse<String> queryBeforeCancel = getJson(
          port,
          "/api/v1/wcs/agv/missions/M202604200001"
      );
      assertThat(queryBeforeCancel.statusCode()).isEqualTo(200);
      JsonNode queryBeforeCancelJson = new ObjectMapper().readTree(queryBeforeCancel.body());
      assertThat(queryBeforeCancelJson.get("data").get("rcs_status").asText()).isEqualTo("RECEIVED");

      HttpResponse<String> cancelResp = postJson(
          port,
          "/api/v1/wcs/agv/missions/M202604200001/cancel",
          ""
      );
      assertThat(cancelResp.statusCode()).isEqualTo(200);
      JsonNode cancelJson = new ObjectMapper().readTree(cancelResp.body());
      assertThat(cancelJson.get("data").get("rcs_status").asText()).isEqualTo("CANCELED");
      assertThat(cancelJson.get("data").get("idem_hit").asBoolean()).isFalse();

      HttpResponse<String> queryAfterCancel = getJson(
          port,
          "/api/v1/wcs/agv/missions/M202604200001"
      );
      assertThat(queryAfterCancel.statusCode()).isEqualTo(200);
      JsonNode queryAfterCancelJson = new ObjectMapper().readTree(queryAfterCancel.body());
      assertThat(queryAfterCancelJson.get("data").get("rcs_status").asText()).isEqualTo("CANCELED");
    }
    finally {
      app.stop();
    }
  }

  @Test
  void shouldReturnIdempotencyConflictCodeForDifferentPayloadOnSameMission() throws Exception {
    Javalin app = RcsIntegrationApplication.createApp();
    app.start(0);
    try {
      int port = app.port();
      String firstBody = """
          {
            "mission_no": "M202604200101",
            "task_no": "T202604200101",
            "from_point": "P_WAIT_IN_01",
            "to_point": "ST_IN_01",
            "pallet_no": "PLT000000123",
            "priority": 30,
            "callback_url": "http://127.0.0.1:8081/api/v1/wcs/agv/events"
          }
          """;
      String conflictingBody = """
          {
            "mission_no": "M202604200101",
            "task_no": "T202604200101",
            "from_point": "P_WAIT_IN_02",
            "to_point": "ST_IN_01",
            "pallet_no": "PLT000000123",
            "priority": 30,
            "callback_url": "http://127.0.0.1:8081/api/v1/wcs/agv/events"
          }
          """;
      HttpResponse<String> firstResp = postJson(port, "/api/v1/wcs/agv/missions", firstBody);
      assertThat(firstResp.statusCode()).isEqualTo(200);

      HttpResponse<String> conflictResp = postJson(
          port,
          "/api/v1/wcs/agv/missions",
          conflictingBody
      );
      assertThat(conflictResp.statusCode()).isEqualTo(409);
      JsonNode conflictJson = new ObjectMapper().readTree(conflictResp.body());
      assertThat(conflictJson.get("code").asText()).isEqualTo("RCS-4009");
    }
    finally {
      app.stop();
    }
  }

  @Test
  void shouldAcceptInboundTaskAndSupportQueryAndCancel() throws Exception {
    Javalin app = RcsIntegrationApplication.createApp();
    app.start(0);
    try {
      int port = app.port();
      String createBody = """
          {
            "biz_task_no": "BIZ-IN-202604210001",
            "from_point": "Point-0020",
            "to_point": "Point-0026",
            "pallet_no": "PLT000000123",
            "priority": 80,
            "callback_url": "http://127.0.0.1:8081/api/v1/wcs/agv/events"
          }
          """;
      HttpResponse<String> createResp = postJson(port, "/api/v1/wcs/inbound/tasks", createBody);
      assertThat(createResp.statusCode()).isEqualTo(200);
      JsonNode createJson = new ObjectMapper().readTree(createResp.body());
      assertThat(createJson.get("data").get("biz_task_no").asText()).isEqualTo("BIZ-IN-202604210001");
      assertThat(createJson.get("data").get("rcs_status").asText()).isEqualTo("RECEIVED");
      assertThat(createJson.get("data").get("idem_hit").asBoolean()).isFalse();

      HttpResponse<String> idemResp = postJson(port, "/api/v1/wcs/inbound/tasks", createBody);
      assertThat(idemResp.statusCode()).isEqualTo(200);
      JsonNode idemJson = new ObjectMapper().readTree(idemResp.body());
      assertThat(idemJson.get("data").get("idem_hit").asBoolean()).isTrue();

      HttpResponse<String> queryResp = getJson(port, "/api/v1/wcs/tasks/BIZ-IN-202604210001");
      assertThat(queryResp.statusCode()).isEqualTo(200);
      JsonNode queryJson = new ObjectMapper().readTree(queryResp.body());
      assertThat(queryJson.get("data").get("rcs_status").asText()).isEqualTo("RECEIVED");

      HttpResponse<String> cancelResp = postJson(
          port,
          "/api/v1/wcs/tasks/BIZ-IN-202604210001/cancel",
          ""
      );
      assertThat(cancelResp.statusCode()).isEqualTo(200);
      JsonNode cancelJson = new ObjectMapper().readTree(cancelResp.body());
      assertThat(cancelJson.get("data").get("rcs_status").asText()).isEqualTo("CANCELED");
      assertThat(cancelJson.get("data").get("idem_hit").asBoolean()).isFalse();
    }
    finally {
      app.stop();
    }
  }

  @Test
  void shouldReturnWaitPlcForInboundTaskAfterFinishedEvent() throws Exception {
    Javalin app = RcsIntegrationApplication.createApp();
    app.start(0);
    try {
      int port = app.port();
      String createBody = """
          {
            "biz_task_no": "BIZ-IN-202604210101",
            "mission_no": "M202604210101",
            "task_no": "T202604210101",
            "from_point": "Point-0020",
            "to_point": "Point-0026",
            "pallet_no": "PLT000000123",
            "priority": 80,
            "callback_url": "http://127.0.0.1:8081/api/v1/wcs/agv/events"
          }
          """;
      HttpResponse<String> createResp = postJson(port, "/api/v1/wcs/inbound/tasks", createBody);
      assertThat(createResp.statusCode()).isEqualTo(200);

      String openTcsEventBody = """
          {
            "eventTime": "2026-04-21T12:35:21Z",
            "currentObjectState": {
              "name": "M202604210101",
              "state": "FINISHED",
              "currentDriveOrderIndex": 1,
              "processingVehicle": "AGV_01",
              "properties": {
                "task_no": "T202604210101"
              }
            }
          }
          """;
      HttpResponse<String> eventResp = postJson(
          port,
          "/api/v1/opentcs/events/transport-orders",
          openTcsEventBody
      );
      assertThat(eventResp.statusCode()).isEqualTo(202);

      HttpResponse<String> queryResp = getJson(port, "/api/v1/wcs/tasks/BIZ-IN-202604210101");
      assertThat(queryResp.statusCode()).isEqualTo(200);
      JsonNode queryJson = new ObjectMapper().readTree(queryResp.body());
      assertThat(queryJson.get("data").get("rcs_status").asText()).isEqualTo("WAIT_PLC");
    }
    finally {
      app.stop();
    }
  }

  @Test
  void shouldCallbackOutboundTaskResultToConfiguredWmsEndpoint() throws Exception {
    AtomicInteger missionCallbackCalls = new AtomicInteger();
    AtomicInteger resultCallbackCalls = new AtomicInteger();
    AtomicReference<String> resultPayload = new AtomicReference<>();
    HttpServer callbackServer = HttpServer.create(new InetSocketAddress(0), 0);
    callbackServer.createContext(
        "/api/v1/wcs/agv/events",
        exchange -> {
          missionCallbackCalls.incrementAndGet();
          byte[] bytes = "{}".getBytes(StandardCharsets.UTF_8);
          exchange.sendResponseHeaders(200, bytes.length);
          exchange.getResponseBody().write(bytes);
          exchange.close();
        }
    );
    callbackServer.createContext(
        "/api/v1/wms/outbound-results",
        exchange -> {
          resultCallbackCalls.incrementAndGet();
          resultPayload.set(new String(exchange.getRequestBody().readAllBytes(), StandardCharsets.UTF_8));
          byte[] bytes = "{}".getBytes(StandardCharsets.UTF_8);
          exchange.sendResponseHeaders(200, bytes.length);
          exchange.getResponseBody().write(bytes);
          exchange.close();
        }
    );
    callbackServer.start();

    int callbackPort = callbackServer.getAddress().getPort();
    System.setProperty("rcs.wms.baseUrl", "http://127.0.0.1:" + callbackPort);
    Javalin app = RcsIntegrationApplication.createApp();
    app.start(0);
    try {
      int port = app.port();
      String createBody = """
          {
            "biz_task_no": "BIZ-OUT-202604220001",
            "mission_no": "M202604220001",
            "task_no": "T202604220001",
            "from_point": "Point-0026",
            "to_point": "Point-0020",
            "pallet_no": "PLT000000123",
            "priority": 80,
            "callback_url": "http://127.0.0.1:%d/api/v1/wcs/agv/events"
          }
          """.formatted(callbackPort);
      HttpResponse<String> createResp = postJson(port, "/api/v1/wcs/outbound/tasks", createBody);
      assertThat(createResp.statusCode()).isEqualTo(200);

      String openTcsEventBody = """
          {
            "eventTime": "2026-04-22T12:35:21Z",
            "currentObjectState": {
              "name": "M202604220001",
              "state": "FINISHED",
              "currentDriveOrderIndex": 1,
              "processingVehicle": "AGV_01",
              "properties": {
                "task_no": "T202604220001"
              }
            }
          }
          """;
      HttpResponse<String> eventResp = postJson(
          port,
          "/api/v1/opentcs/events/transport-orders",
          openTcsEventBody
      );
      assertThat(eventResp.statusCode()).isEqualTo(202);

      HttpResponse<String> queryResp = getJson(port, "/api/v1/wcs/tasks/BIZ-OUT-202604220001");
      assertThat(queryResp.statusCode()).isEqualTo(200);
      JsonNode queryJson = new ObjectMapper().readTree(queryResp.body());
      assertThat(queryJson.get("data").get("rcs_status").asText()).isEqualTo("DONE");

      waitUntil(() -> resultCallbackCalls.get() > 0, 3000);
      assertThat(missionCallbackCalls.get()).isGreaterThan(0);
      assertThat(resultPayload.get()).contains("\"biz_task_no\":\"BIZ-OUT-202604220001\"");
      assertThat(resultPayload.get()).contains("\"result_type\":\"DONE\"");
      assertThat(resultPayload.get()).contains("\"task_type\":\"OUTBOUND\"");
    }
    finally {
      app.stop();
      callbackServer.stop(0);
      System.clearProperty("rcs.wms.baseUrl");
    }
  }

  private void waitUntil(Condition condition, long timeoutMillis) throws InterruptedException {
    long deadline = System.currentTimeMillis() + timeoutMillis;
    while (System.currentTimeMillis() < deadline) {
      if (condition.matches()) {
        return;
      }
      Thread.sleep(20L);
    }
    throw new AssertionError("Condition not met within " + timeoutMillis + "ms");
  }

  @FunctionalInterface
  private interface Condition {

    boolean matches();
  }

  private HttpResponse<String> postJson(int port, String path, String body)
      throws IOException, InterruptedException {
    HttpClient client = HttpClient.newHttpClient();
    HttpRequest request = HttpRequest.newBuilder()
        .uri(URI.create("http://127.0.0.1:" + port + path))
        .header("Content-Type", "application/json")
        .POST(HttpRequest.BodyPublishers.ofString(body))
        .build();
    return client.send(request, HttpResponse.BodyHandlers.ofString());
  }

  private HttpResponse<String> getJson(int port, String path)
      throws IOException, InterruptedException {
    HttpClient client = HttpClient.newHttpClient();
    HttpRequest request = HttpRequest.newBuilder()
        .uri(URI.create("http://127.0.0.1:" + port + path))
        .header("Accept", "application/json")
        .GET()
        .build();
    return client.send(request, HttpResponse.BodyHandlers.ofString());
  }
}
