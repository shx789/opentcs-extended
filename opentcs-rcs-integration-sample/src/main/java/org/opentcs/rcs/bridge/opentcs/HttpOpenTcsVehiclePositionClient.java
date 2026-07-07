// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.opentcs;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.Map;
import java.util.Objects;

/**
 * Updates a vehicle's current point position through the openTCS service web API.
 */
public class HttpOpenTcsVehiclePositionClient
    implements OpenTcsVehiclePositionClient {

  private final HttpClient httpClient;
  private final ObjectMapper objectMapper;
  private final URI baseUri;
  private final Duration timeout;
  private final String bearerToken;

  public HttpOpenTcsVehiclePositionClient(
      HttpClient httpClient,
      ObjectMapper objectMapper,
      URI baseUri,
      Duration timeout,
      String bearerToken
  ) {
    this.httpClient = Objects.requireNonNull(httpClient, "httpClient");
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
    this.baseUri = normalizeBaseUri(Objects.requireNonNull(baseUri, "baseUri"));
    this.timeout = Objects.requireNonNull(timeout, "timeout");
    this.bearerToken = normalizeNullable(bearerToken);
  }

  @Override
  public void updateVehiclePosition(String vehicleName, String pointName) {
    Objects.requireNonNull(vehicleName, "vehicleName");
    Objects.requireNonNull(pointName, "pointName");
    HttpRequest.Builder requestBuilder = HttpRequest.newBuilder(positionUri(vehicleName))
        .timeout(timeout)
        .header("Content-Type", "application/json")
        .PUT(HttpRequest.BodyPublishers.ofString(toJson(pointName), StandardCharsets.UTF_8));
    if (bearerToken != null) {
      requestBuilder.header("Authorization", "Bearer " + bearerToken);
    }

    try {
      HttpResponse<String> response = httpClient.send(
          requestBuilder.build(),
          HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8)
      );
      if (response.statusCode() < 200 || response.statusCode() >= 300) {
        throw new OpenTcsClientException(
            "openTCS vehicle position update failed: HTTP "
                + response.statusCode()
                + " body="
                + response.body()
        );
      }
    }
    catch (IOException exc) {
      throw new OpenTcsClientException("Could not update openTCS vehicle position", exc);
    }
    catch (InterruptedException exc) {
      Thread.currentThread().interrupt();
      throw new OpenTcsClientException("Interrupted while updating openTCS vehicle position", exc);
    }
  }

  private URI positionUri(String vehicleName) {
    String encodedVehicleName = URLEncoder.encode(vehicleName, StandardCharsets.UTF_8);
    return baseUri.resolve("v1/vehicles/" + encodedVehicleName + "/position");
  }

  private String toJson(String pointName) {
    try {
      return objectMapper.writeValueAsString(Map.of("pointName", pointName));
    }
    catch (JsonProcessingException exc) {
      throw new OpenTcsClientException("Could not serialize vehicle position update", exc);
    }
  }

  private static URI normalizeBaseUri(URI uri) {
    String value = uri.toString();
    return URI.create(value.endsWith("/") ? value : value + "/");
  }

  private static String normalizeNullable(String value) {
    return value == null || value.isBlank() ? null : value.trim();
  }
}
