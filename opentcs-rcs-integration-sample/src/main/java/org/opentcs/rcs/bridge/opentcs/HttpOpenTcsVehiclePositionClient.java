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
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * Updates a vehicle's current point position through the openTCS service web API.
 */
public class HttpOpenTcsVehiclePositionClient
    implements
      OpenTcsVehiclePositionClient {

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

    try {
      HttpResponse<String> response = send(commAdapterMessageRequest(vehicleName, pointName));
      if (response.statusCode() < 200 || response.statusCode() >= 300) {
        throw new OpenTcsClientException(
            "openTCS commAdapter position update failed: HTTP "
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

  private HttpResponse<String> send(HttpRequest request)
      throws IOException,
        InterruptedException {
    return httpClient.send(request, HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
  }

  private HttpRequest positionRequest(String vehicleName, String pointName) {
    return requestBuilder(positionUri(vehicleName))
        .PUT(HttpRequest.BodyPublishers.ofString(toPositionJson(pointName), StandardCharsets.UTF_8))
        .build();
  }

  private HttpRequest commAdapterMessageRequest(String vehicleName, String pointName) {
    return requestBuilder(commAdapterMessageUri(vehicleName))
        .POST(
            HttpRequest.BodyPublishers.ofString(
                toCommAdapterMessageJson(pointName),
                StandardCharsets.UTF_8
            )
        )
        .build();
  }

  private HttpRequest.Builder requestBuilder(URI uri) {
    HttpRequest.Builder requestBuilder = HttpRequest.newBuilder(uri)
        .timeout(timeout)
        .header("Content-Type", "application/json");
    if (bearerToken != null) {
      requestBuilder.header("Authorization", "Bearer " + bearerToken);
    }
    return requestBuilder;
  }

  private URI positionUri(String vehicleName) {
    String encodedVehicleName = URLEncoder.encode(vehicleName, StandardCharsets.UTF_8);
    return baseUri.resolve("v1/vehicles/" + encodedVehicleName + "/position");
  }

  private URI commAdapterMessageUri(String vehicleName) {
    String encodedVehicleName = URLEncoder.encode(vehicleName, StandardCharsets.UTF_8);
    return baseUri.resolve("v1/vehicles/" + encodedVehicleName + "/commAdapter/message");
  }

  private String toPositionJson(String pointName) {
    try {
      return objectMapper.writeValueAsString(Map.of("pointName", pointName));
    }
    catch (JsonProcessingException exc) {
      throw new OpenTcsClientException("Could not serialize vehicle position update", exc);
    }
  }

  private String toCommAdapterMessageJson(String pointName) {
    try {
      return objectMapper.writeValueAsString(
          Map.of(
              "type",
              "tcs:virtualVehicle:setPosition",
              "parameters",
              List.of(Map.of("key", "position", "value", pointName))
          )
      );
    }
    catch (JsonProcessingException exc) {
      throw new OpenTcsClientException("Could not serialize vehicle position fallback update", exc);
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
