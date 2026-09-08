// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpServer;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.URI;
import java.net.http.HttpClient;
import java.nio.charset.StandardCharsets;
import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.time.ZoneOffset;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import org.junit.jupiter.api.Test;

class HttpCallbackSenderTest {

  @Test
  void shouldResolveRelativeUrlAndAttachAuthAndSignatureHeaders()
      throws Exception {
    HttpServer server = HttpServer.create(new InetSocketAddress(0), 0);
    AtomicReference<String> path = new AtomicReference<>();
    AtomicReference<String> authorization = new AtomicReference<>();
    AtomicReference<String> timestamp = new AtomicReference<>();
    AtomicReference<String> signature = new AtomicReference<>();
    AtomicReference<String> body = new AtomicReference<>();
    server.createContext(
        "/api/v1/wcs/agv/events",
        exchange -> {
          captureRequest(exchange, path, authorization, timestamp, signature, body);
          respond(exchange, 200, "{}");
        }
    );
    server.start();

    try {
      Instant fixedInstant = Instant.parse("2026-04-14T10:11:12Z");
      HttpCallbackSender sender = new HttpCallbackSender(
          HttpClient.newHttpClient(),
          URI.create("http://127.0.0.1:" + server.getAddress().getPort()),
          Duration.ofSeconds(2),
          "wcs-token",
          "secret-key",
          Clock.fixed(fixedInstant, ZoneOffset.UTC),
          "X-Test-Timestamp",
          "X-Test-Signature"
      );

      sender.send("/api/v1/wcs/agv/events", "{\"eventType\":\"DROPPED\"}");

      assertThat(path.get()).isEqualTo("/api/v1/wcs/agv/events");
      assertThat(authorization.get()).isEqualTo("Bearer wcs-token");
      assertThat(timestamp.get()).isEqualTo(Long.toString(fixedInstant.toEpochMilli()));
      assertThat(signature.get()).isEqualTo(
          expectedSignature(timestamp.get(), "{\"eventType\":\"DROPPED\"}", "secret-key")
      );
      assertThat(body.get()).isEqualTo("{\"eventType\":\"DROPPED\"}");
    }
    finally {
      server.stop(0);
    }
  }

  @Test
  void shouldSupportAbsoluteCallbackUrlWithoutBaseUri()
      throws Exception {
    HttpServer server = HttpServer.create(new InetSocketAddress(0), 0);
    AtomicInteger calls = new AtomicInteger();
    server.createContext(
        "/callback",
        exchange -> {
          calls.incrementAndGet();
          respond(exchange, 204, "");
        }
    );
    server.start();

    try {
      HttpCallbackSender sender = new HttpCallbackSender(
          HttpClient.newHttpClient(),
          null,
          Duration.ofSeconds(2),
          null,
          null
      );

      sender.send("http://127.0.0.1:" + server.getAddress().getPort() + "/callback", "{}");

      assertThat(calls.get()).isEqualTo(1);
    }
    finally {
      server.stop(0);
    }
  }

  @Test
  void shouldThrowWhenWcsReturnsErrorStatus()
      throws Exception {
    HttpServer server = HttpServer.create(new InetSocketAddress(0), 0);
    server.createContext("/callback", exchange -> respond(exchange, 500, "{\"error\":\"down\"}"));
    server.start();

    try {
      HttpCallbackSender sender = new HttpCallbackSender(
          HttpClient.newHttpClient(),
          URI.create("http://127.0.0.1:" + server.getAddress().getPort()),
          Duration.ofSeconds(2),
          null,
          null
      );

      assertThatThrownBy(() -> sender.send("/callback", "{}"))
          .isInstanceOf(IllegalStateException.class)
          .hasMessageContaining("status=500");
    }
    finally {
      server.stop(0);
    }
  }

  private void captureRequest(
      HttpExchange exchange,
      AtomicReference<String> path,
      AtomicReference<String> authorization,
      AtomicReference<String> timestamp,
      AtomicReference<String> signature,
      AtomicReference<String> body
  )
      throws IOException {
    path.set(exchange.getRequestURI().getRawPath());
    authorization.set(exchange.getRequestHeaders().getFirst("Authorization"));
    timestamp.set(exchange.getRequestHeaders().getFirst("X-Test-Timestamp"));
    signature.set(exchange.getRequestHeaders().getFirst("X-Test-Signature"));
    body.set(new String(exchange.getRequestBody().readAllBytes(), StandardCharsets.UTF_8));
  }

  private void respond(HttpExchange exchange, int statusCode, String responseBody)
      throws IOException {
    byte[] bytes = responseBody.getBytes(StandardCharsets.UTF_8);
    exchange.getResponseHeaders().add("Content-Type", "application/json");
    exchange.sendResponseHeaders(statusCode, bytes.length);
    exchange.getResponseBody().write(bytes);
    exchange.close();
  }

  private String expectedSignature(String timestamp, String payload, String secret)
      throws Exception {
    String content = timestamp + "." + payload;
    Mac mac = Mac.getInstance("HmacSHA256");
    mac.init(new SecretKeySpec(secret.getBytes(StandardCharsets.UTF_8), "HmacSHA256"));
    return java.util.HexFormat.of().formatHex(
        mac.doFinal(content.getBytes(StandardCharsets.UTF_8))
    );
  }
}
