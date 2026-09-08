// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.util.HexFormat;
import java.util.Objects;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

/**
 * HTTP callback sender with optional bearer auth and HMAC signature.
 */
public class HttpCallbackSender
    implements
      CallbackSender {

  private static final String HMAC_ALGORITHM = "HmacSHA256";
  private static final String DEFAULT_TIMESTAMP_HEADER = "X-Callback-Timestamp";
  private static final String DEFAULT_SIGNATURE_HEADER = "X-Callback-Signature";

  private final HttpClient httpClient;
  private final URI callbackBaseUri;
  private final Duration requestTimeout;
  private final String bearerToken;
  private final String signatureSecret;
  private final Clock clock;
  private final String timestampHeader;
  private final String signatureHeader;

  public HttpCallbackSender(
      HttpClient httpClient,
      URI callbackBaseUri,
      Duration requestTimeout,
      String bearerToken,
      String signatureSecret
  ) {
    this(
        httpClient,
        callbackBaseUri,
        requestTimeout,
        bearerToken,
        signatureSecret,
        Clock.systemUTC(),
        DEFAULT_TIMESTAMP_HEADER,
        DEFAULT_SIGNATURE_HEADER
    );
  }

  HttpCallbackSender(
      HttpClient httpClient,
      URI callbackBaseUri,
      Duration requestTimeout,
      String bearerToken,
      String signatureSecret,
      Clock clock,
      String timestampHeader,
      String signatureHeader
  ) {
    this.httpClient = Objects.requireNonNull(httpClient, "httpClient");
    this.callbackBaseUri = callbackBaseUri;
    this.requestTimeout = requirePositive(requestTimeout, "requestTimeout");
    this.bearerToken = normalizeToken(bearerToken);
    this.signatureSecret = normalizeToken(signatureSecret);
    this.clock = Objects.requireNonNull(clock, "clock");
    this.timestampHeader = requireNonBlank(timestampHeader, "timestampHeader");
    this.signatureHeader = requireNonBlank(signatureHeader, "signatureHeader");
  }

  @Override
  public void send(String callbackUrl, String payloadJson)
      throws Exception {
    URI requestUri = resolveCallbackUri(callbackUrl);
    String payload = requireNonBlank(payloadJson, "payloadJson");
    HttpRequest.Builder requestBuilder = HttpRequest.newBuilder(requestUri)
        .timeout(requestTimeout)
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .POST(HttpRequest.BodyPublishers.ofString(payload));

    if (bearerToken != null) {
      requestBuilder.header("Authorization", "Bearer " + bearerToken);
    }
    if (signatureSecret != null) {
      String timestamp = Long.toString(Instant.now(clock).toEpochMilli());
      String signature = signPayload(timestamp, payload);
      requestBuilder.header(timestampHeader, timestamp);
      requestBuilder.header(signatureHeader, signature);
    }

    HttpResponse<String> response = sendRequest(requestBuilder.build());
    if (response.statusCode() < 200 || response.statusCode() >= 300) {
      throw new IllegalStateException(
          "WCS callback failed. status=" + response.statusCode() + ", body=" + safeBody(
              response.body()
          )
      );
    }
  }

  private HttpResponse<String> sendRequest(HttpRequest request) {
    try {
      return httpClient.send(request, HttpResponse.BodyHandlers.ofString());
    }
    catch (InterruptedException exc) {
      Thread.currentThread().interrupt();
      throw new IllegalStateException("Interrupted while sending callback", exc);
    }
    catch (IOException exc) {
      throw new IllegalStateException("Could not send callback request", exc);
    }
  }

  private URI resolveCallbackUri(String callbackUrl) {
    String value = requireNonBlank(callbackUrl, "callbackUrl");
    URI uri = URI.create(value);
    if (uri.isAbsolute()) {
      return uri;
    }
    if (callbackBaseUri == null) {
      throw new IllegalArgumentException(
          "callbackUrl must be absolute when callbackBaseUri is not configured"
      );
    }
    return callbackBaseUri.resolve(uri);
  }

  private String signPayload(String timestamp, String payloadJson)
      throws Exception {
    String content = timestamp + "." + payloadJson;
    Mac mac = Mac.getInstance(HMAC_ALGORITHM);
    mac.init(new SecretKeySpec(signatureSecret.getBytes(StandardCharsets.UTF_8), HMAC_ALGORITHM));
    return HexFormat.of().formatHex(mac.doFinal(content.getBytes(StandardCharsets.UTF_8)));
  }

  private static Duration requirePositive(Duration duration, String fieldName) {
    Objects.requireNonNull(duration, fieldName);
    if (duration.isZero() || duration.isNegative()) {
      throw new IllegalArgumentException(fieldName + " must be positive");
    }
    return duration;
  }

  private static String requireNonBlank(String value, String fieldName) {
    if (value == null || value.isBlank()) {
      throw new IllegalArgumentException(fieldName + " must not be blank");
    }
    return value;
  }

  private static String normalizeToken(String token) {
    if (token == null || token.isBlank()) {
      return null;
    }
    return token.trim();
  }

  private static String safeBody(String body) {
    if (body == null || body.isBlank()) {
      return "<empty>";
    }
    return body;
  }
}
