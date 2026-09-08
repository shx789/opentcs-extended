// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.callback;

import java.time.Duration;
import java.util.Objects;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.TimeUnit;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Periodically dispatches due callback outbox entries.
 */
public class CallbackRetryScheduler {

  private static final Logger LOG = LoggerFactory.getLogger(CallbackRetryScheduler.class);

  private final CallbackRetryProcessor processor;
  private final int batchSize;
  private final Duration interval;
  private final ScheduledExecutorService executor;

  public CallbackRetryScheduler(
      CallbackRetryProcessor processor,
      int batchSize,
      Duration interval
  ) {
    this.processor = Objects.requireNonNull(processor, "processor");
    if (batchSize < 1) {
      throw new IllegalArgumentException("batchSize must be greater than 0");
    }
    this.batchSize = batchSize;
    this.interval = requirePositive(interval, "interval");
    this.executor = Executors.newSingleThreadScheduledExecutor(new DaemonThreadFactory());
  }

  public synchronized void start() {
    executor.scheduleWithFixedDelay(
        this::dispatchSafely,
        0L,
        interval.toMillis(),
        TimeUnit.MILLISECONDS
    );
  }

  public synchronized void stop() {
    executor.shutdownNow();
    try {
      executor.awaitTermination(5, TimeUnit.SECONDS);
    }
    catch (InterruptedException exc) {
      Thread.currentThread().interrupt();
    }
  }

  private void dispatchSafely() {
    try {
      processor.processDue(batchSize);
    }
    catch (RuntimeException exc) {
      LOG.warn("Callback retry tick failed: {}", exc.getMessage());
    }
  }

  private static Duration requirePositive(Duration value, String fieldName) {
    Objects.requireNonNull(value, fieldName);
    if (value.isZero() || value.isNegative()) {
      throw new IllegalArgumentException(fieldName + " must be positive");
    }
    return value;
  }

  private static final class DaemonThreadFactory
      implements
        ThreadFactory {

    @Override
    public Thread newThread(Runnable runnable) {
      Thread thread = new Thread(runnable, "callback-retry-scheduler");
      thread.setDaemon(true);
      return thread;
    }
  }
}
