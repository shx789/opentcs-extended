// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv.mapping;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Loads AGV/openTCS point mappings from the generated mapping JSON file.
 */
public class AgvPointMappingLoader {

  private final ObjectMapper objectMapper;

  public AgvPointMappingLoader(ObjectMapper objectMapper) {
    this.objectMapper = Objects.requireNonNull(objectMapper, "objectMapper");
  }

  public List<AgvPointMapping> load(Path path) {
    Objects.requireNonNull(path, "path");
    try {
      JsonNode root = objectMapper.readTree(path.toFile());
      JsonNode points = root.path("points");
      if (!points.isArray()) {
        throw new IllegalArgumentException("Mapping file must contain a points array: " + path);
      }
      List<AgvPointMapping> result = new ArrayList<>();
      for (JsonNode point : points) {
        String name = text(point.path("name"));
        JsonNode rosPose = point.path("ros_pose");
        if (name == null || !rosPose.isObject()) {
          continue;
        }
        result.add(new AgvPointMapping(
            name,
            rosPose.path("x").asDouble(),
            rosPose.path("y").asDouble(),
            rosPose.path("yaw").asDouble(0.0)
        ));
      }
      return result;
    }
    catch (IOException exc) {
      throw new IllegalArgumentException("Could not load AGV point mapping file: " + path, exc);
    }
  }

  private static String text(JsonNode node) {
    return node.isTextual() && !node.asText().isBlank() ? node.asText().trim() : null;
  }
}
