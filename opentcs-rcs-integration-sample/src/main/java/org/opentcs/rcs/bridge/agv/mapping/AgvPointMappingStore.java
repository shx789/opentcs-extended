// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.bridge.agv.mapping;

import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.function.Function;
import java.util.stream.Collectors;

/**
 * Looks up point mappings by name or by nearest ROS map position.
 */
public class AgvPointMappingStore {

  private final Map<String, AgvPointMapping> mappingsByName;
  private final double nearestPointThresholdMeters;

  public AgvPointMappingStore(
      List<AgvPointMapping> mappings,
      double nearestPointThresholdMeters
  ) {
    Objects.requireNonNull(mappings, "mappings");
    if (nearestPointThresholdMeters < 0) {
      throw new IllegalArgumentException(
          "nearestPointThresholdMeters must be greater than or equal to zero"
      );
    }
    this.mappingsByName = mappings.stream()
        .collect(
            Collectors.toUnmodifiableMap(
                mapping -> normalize(mapping.pointName()),
                Function.identity(),
                (left, right) -> right
            )
        );
    this.nearestPointThresholdMeters = nearestPointThresholdMeters;
  }

  public Optional<AgvPointMapping> findByName(String pointName) {
    if (pointName == null || pointName.isBlank()) {
      return Optional.empty();
    }
    if (mappingsByName.isEmpty()) {
      return Optional.of(new AgvPointMapping(pointName.trim(), Double.NaN, Double.NaN, Double.NaN));
    }
    return Optional.ofNullable(mappingsByName.get(normalize(pointName)));
  }

  public Optional<AgvPointMapping> findNearest(Double rosX, Double rosY) {
    if (rosX == null || rosY == null || mappingsByName.isEmpty()) {
      return Optional.empty();
    }
    AgvPointMapping best = null;
    double bestDistance = Double.MAX_VALUE;
    for (AgvPointMapping mapping : mappingsByName.values()) {
      double distance = Math.hypot(rosX - mapping.rosX(), rosY - mapping.rosY());
      if (distance < bestDistance) {
        best = mapping;
        bestDistance = distance;
      }
    }
    if (best != null && bestDistance <= nearestPointThresholdMeters) {
      return Optional.of(best);
    }
    return Optional.empty();
  }

  private static String normalize(String value) {
    return value.trim().toUpperCase(Locale.ROOT);
  }
}
