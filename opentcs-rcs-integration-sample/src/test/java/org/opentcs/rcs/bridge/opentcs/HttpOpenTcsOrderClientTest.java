// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpServer;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.URI;
import java.net.http.HttpClient;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.jupiter.api.Test;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsDestination;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsProperty;
import org.opentcs.rcs.bridge.opentcs.dto.OpenTcsTransportOrderReq;

class HttpOpenTcsOrderClientTest {

  @Test
  void shouldSendOrderWithBearerToken() throws Exception {
    HttpServer server = HttpServer.create(new InetSocketAddress(0), 0);
    AtomicReference<String> methodRef = new AtomicReference<>();
    AtomicReference<String> rawPathRef = new AtomicReference<>();
    AtomicReference<String> authHeaderRef = new AtomicReference<>();
    AtomicReference<String> bodyRef = new AtomicReference<>();
    server.createContext(
        "/v1/transportOrders",
        exchange -> {
          captureRequest(exchange, methodRef, rawPathRef, authHeaderRef, bodyRef);
          respond(exchange, 200, "{}");
        }
    );
    server.start();

    try {
      HttpOpenTcsOrderClient client = new HttpOpenTcsOrderClient(
          HttpClient.newHttpClient(),
          new ObjectMapper(),
          URI.create("http://127.0.0.1:" + server.getAddress().getPort()),
          Duration.ofSeconds(2),
          1,
          Duration.ZERO,
          "token-123"
      );

      client.createTransportOrder("M 1", samplePayload());

      assertThat(methodRef.get()).isEqualTo("POST");
      assertThat(rawPathRef.get()).isEqualTo("/v1/transportOrders/M%201");
      assertThat(authHeaderRef.get()).isEqualTo("Bearer token-123");
      assertThat(bodyRef.get()).contains("\"type\":\"Transport\"");
    }
    finally {
      server.stop(0);
    }
  }

  @Test
  void shouldRetryWhenServerReturns5xx() throws Exception {
    HttpServer server = HttpServer.create(new InetSocketAddress(0), 0);
    AtomicInteger attempts = new AtomicInteger();
    server.createContext(
        "/v1/transportOrders",
        exchange -> {
          int current = attempts.incrementAndGet();
          if (current < 3) {
            respond(exchange, 503, "{\"error\":\"busy\"}");
            return;
          }
          respond(exchange, 200, "{}");
        }
    );
    server.start();

    try {
      HttpOpenTcsOrderClient client = new HttpOpenTcsOrderClient(
          HttpClient.newHttpClient(),
          new ObjectMapper(),
          URI.create("http://127.0.0.1:" + server.getAddress().getPort()),
          Duration.ofSeconds(2),
          3,
          Duration.ofMillis(5),
          null
      );

      client.createTransportOrder("M2", samplePayload());

      assertThat(attempts.get()).isEqualTo(3);
    }
    finally {
      server.stop(0);
    }
  }

  @Test
  void shouldNotRetryWhenServerReturns4xx() throws Exception {
    HttpServer server = HttpServer.create(new InetSocketAddress(0), 0);
    AtomicInteger attempts = new AtomicInteger();
    server.createContext(
        "/v1/transportOrders",
        exchange -> {
          attempts.incrementAndGet();
          respond(exchange, 400, "{\"error\":\"bad request\"}");
        }
    );
    server.start();

    try {
      HttpOpenTcsOrderClient client = new HttpOpenTcsOrderClient(
          HttpClient.newHttpClient(),
          new ObjectMapper(),
          URI.create("http://127.0.0.1:" + server.getAddress().getPort()),
          Duration.ofSeconds(2),
          3,
          Duration.ofMillis(5),
          null
      );

      assertThatThrownBy(() -> client.createTransportOrder("M3", samplePayload()))
          .isInstanceOf(OpenTcsClientException.class)
          .hasMessageContaining("status=400");
      assertThat(attempts.get()).isEqualTo(1);
    }
    finally {
      server.stop(0);
    }
  }

  private void captureRequest(
      HttpExchange exchange,
      AtomicReference<String> methodRef,
      AtomicReference<String> rawPathRef,
      AtomicReference<String> authHeaderRef,
      AtomicReference<String> bodyRef
  ) throws IOException {
    methodRef.set(exchange.getRequestMethod());
    rawPathRef.set(exchange.getRequestURI().getRawPath());
    authHeaderRef.set(exchange.getRequestHeaders().getFirst("Authorization"));
    bodyRef.set(new String(exchange.getRequestBody().readAllBytes(), StandardCharsets.UTF_8));
  }

  private void respond(HttpExchange exchange, int statusCode, String body) throws IOException {
    byte[] bytes = body.getBytes(StandardCharsets.UTF_8);
    exchange.getResponseHeaders().add("Content-Type", "application/json");
    exchange.sendResponseHeaders(statusCode, bytes.length);
    exchange.getResponseBody().write(bytes);
    exchange.close();
  }

  private OpenTcsTransportOrderReq samplePayload() {
    return new OpenTcsTransportOrderReq(
        false,
        "Transport",
        "2026-04-13T23:00:00Z",
        List.of(new OpenTcsDestination("ST_IN_01", "NOP", List.of())),
        List.of(new OpenTcsProperty("missionNo", "M1"))
    );
  }
}
