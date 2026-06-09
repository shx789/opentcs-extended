// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import com.fasterxml.jackson.datatype.jsr310.JavaTimeModule;
import io.javalin.Javalin;
import io.javalin.config.JavalinConfig;
import static io.javalin.apibuilder.ApiBuilder.get;
import static io.javalin.apibuilder.ApiBuilder.post;
import java.net.URI;
import java.net.http.HttpClient;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.time.Duration;
import java.util.Locale;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import org.opentcs.rcs.api.wcs.WcsDemoHttpHandlers;
import org.opentcs.rcs.api.wcs.WcsMissionHttpHandlers;
import org.opentcs.rcs.api.wcs.WcsMissionService;
import org.opentcs.rcs.api.wcs.WcsTaskHttpHandlers;
import org.opentcs.rcs.api.wcs.WcsTaskService;
import org.opentcs.rcs.api.wcs.WmsTaskResultService;
import org.opentcs.rcs.api.wcs.ResourceNotFoundException;
import org.opentcs.rcs.api.wcs.TaskStateConflictException;
import org.opentcs.rcs.api.wcs.AgvMonitorHttpHandlers;
import org.opentcs.rcs.agvcommand.AgvCommandHttpHandlers;
import org.opentcs.rcs.agvcommand.AgvCommandOutboxService;
import org.opentcs.rcs.agvcommand.AgvCommandOutboxStore;
import org.opentcs.rcs.agvcommand.AgvCommandRetryProcessor;
import org.opentcs.rcs.agvcommand.AgvCommandRetryScheduler;
import org.opentcs.rcs.agvcommand.FileAgvCommandOutboxStore;
import org.opentcs.rcs.agvcommand.InMemoryAgvCommandOutboxStore;
import org.opentcs.rcs.bridge.agv.AgvCommandPublisher;
import org.opentcs.rcs.bridge.agv.AgvMqttStatusEventConsumer;
import org.opentcs.rcs.bridge.agv.AgvMqttStatusPayloadParser;
import org.opentcs.rcs.bridge.agv.AgvMqttStatusSubscriber;
import org.opentcs.rcs.bridge.agv.MqttAgvRobotControlPublisher;
import org.opentcs.rcs.bridge.opentcs.HttpOpenTcsOrderClient;
import org.opentcs.rcs.bridge.opentcs.InMemoryOpenTcsOrderClient;
import org.opentcs.rcs.bridge.opentcs.OpenTcsClientException;
import org.opentcs.rcs.bridge.opentcs.OpenTcsEventHttpHandlers;
import org.opentcs.rcs.bridge.opentcs.OpenTcsEventProjector;
import org.opentcs.rcs.bridge.opentcs.OpenTcsOrderClient;
import org.opentcs.rcs.bridge.opentcs.OpenTcsPayloadMapper;
import org.opentcs.rcs.bridge.opentcs.OpenTcsSseEventConsumer;
import org.opentcs.rcs.bridge.opentcs.OpenTcsSsePayloadParser;
import org.opentcs.rcs.bridge.opentcs.OpenTcsSseTransportOrderSubscriber;
import org.opentcs.rcs.callback.CallbackOutboxService;
import org.opentcs.rcs.callback.CallbackOutboxStore;
import org.opentcs.rcs.callback.CallbackRetryProcessor;
import org.opentcs.rcs.callback.CallbackRetryScheduler;
import org.opentcs.rcs.callback.CallbackSender;
import org.opentcs.rcs.callback.FileCallbackOutboxStore;
import org.opentcs.rcs.callback.HttpCallbackSender;
import org.opentcs.rcs.callback.InMemoryCallbackOutboxStore;
import org.opentcs.rcs.api.dto.ApiResponse;
import org.opentcs.rcs.core.idem.FileIdempotencyStore;
import org.opentcs.rcs.core.idem.IdempotencyConflictException;
import org.opentcs.rcs.core.idem.IdempotencyService;
import org.opentcs.rcs.core.idem.IdempotencyStore;
import org.opentcs.rcs.core.idem.InMemoryIdempotencyStore;
import org.opentcs.rcs.core.mission.FileMissionStore;
import org.opentcs.rcs.core.mission.InMemoryMissionStore;
import org.opentcs.rcs.core.mission.MissionStore;
import org.opentcs.rcs.core.task.FileTaskStore;
import org.opentcs.rcs.core.task.InMemoryTaskStore;
import org.opentcs.rcs.core.task.TaskStore;

/**
 * Bootstrap class for local RCS integration sample runtime.
 */
public final class RcsIntegrationApplication {

  private RcsIntegrationApplication() {
  }

  public static void main(String[] args) {
    int port = resolvePort(args);
    AppRuntime runtime = createRuntime();
    Runtime.getRuntime().addShutdownHook(
        new Thread(() -> {
          runtime.stopBackgroundWorkers();
          runtime.app().stop();
        }, "rcs-runtime-shutdown")
    );
    runtime.app().start(port);
    runtime.startBackgroundWorkers();
  }

  public static Javalin createApp() {
    return createRuntime().app();
  }

  private static AppRuntime createRuntime() {
    ObjectMapper objectMapper = new ObjectMapper()
        .registerModule(new JavaTimeModule())
        .disable(SerializationFeature.WRITE_DATES_AS_TIMESTAMPS);
    MissionStore missionStore = createMissionStore(objectMapper);
    TaskStore taskStore = createTaskStore(objectMapper);
    IdempotencyStore idempotencyStore = createIdempotencyStore(objectMapper);
    CallbackOutboxStore callbackOutboxStore = createCallbackOutboxStore(objectMapper);
    AgvCommandOutboxStore agvCommandOutboxStore = createAgvCommandOutboxStore(objectMapper);
    CallbackOutboxService callbackOutboxService = new CallbackOutboxService(
        callbackOutboxStore,
        objectMapper
    );
    List<WcsDemoHttpHandlers.DemoCallbackRecord> demoCallbackRecords = new ArrayList<>();
    CallbackRetryProcessor callbackRetryProcessor = new CallbackRetryProcessor(
        callbackOutboxStore,
        createCallbackSender()
    );
    WmsTaskResultService wmsTaskResultService = new WmsTaskResultService(
        callbackOutboxService,
        resolveWmsBaseUrl().orElse(null)
    );
    OpenTcsSseEventConsumer openTcsEventConsumer = new OpenTcsSseEventConsumer(
        new OpenTcsEventProjector(),
        callbackOutboxService,
        missionStore,
        taskStore,
        wmsTaskResultService
    );
    OpenTcsSsePayloadParser ssePayloadParser = new OpenTcsSsePayloadParser(objectMapper);
    OpenTcsOrderClient orderClient = createOpenTcsOrderClient(objectMapper);
    AgvCommandRuntime agvCommandRuntime = createAgvCommandRuntime(objectMapper, agvCommandOutboxStore);
    AgvCommandPublisher agvCommandPublisher = agvCommandRuntime.publisher();
    WcsMissionService missionService = new WcsMissionService(
        new IdempotencyService(idempotencyStore, objectMapper),
        new OpenTcsPayloadMapper(),
        orderClient,
        objectMapper,
        missionStore,
        agvCommandPublisher
    );
    WcsTaskService taskService = new WcsTaskService(
        new IdempotencyService(idempotencyStore, objectMapper),
        missionService,
        taskStore,
        objectMapper
    );
    int dispatchBatchSize = resolveInt(
        "rcs.callback.dispatchBatchSize",
        "RCS_CALLBACK_DISPATCH_BATCH_SIZE",
        100
    );
    CallbackRetryScheduler callbackRetryScheduler = new CallbackRetryScheduler(
        callbackRetryProcessor,
        dispatchBatchSize,
        Duration.ofMillis(resolveLong(
            "rcs.callback.retryTickMillis",
            "RCS_CALLBACK_RETRY_TICK_MILLIS",
            1000L
        ))
    );
    Optional<OpenTcsSseTransportOrderSubscriber> sseSubscriber = createOpenTcsSseSubscriber(
        ssePayloadParser,
        openTcsEventConsumer,
        callbackRetryProcessor,
        dispatchBatchSize
    );
    Optional<AgvMqttStatusSubscriber> agvMqttSubscriber = createAgvMqttSubscriber(
        objectMapper,
        callbackOutboxService,
        missionStore,
        taskStore,
        wmsTaskResultService,
        callbackRetryProcessor,
        dispatchBatchSize
    );
    Consumer<JavalinConfig> config = cfg -> {
      cfg.startup.showJavalinBanner = false;
      cfg.routes.apiBuilder(() -> {
        get(
            "/demo/agv-monitor",
            AgvMonitorHttpHandlers.pageHandler()
        );
        get(
            "/api/v1/wcs/agv/runtime",
            AgvMonitorHttpHandlers.runtimeStatusHandler()
        );
        get(
            "/demo/wcs",
            WcsDemoHttpHandlers.pageHandler(
                firstNonBlank(
                    System.getProperty("rcs.openTcs.baseUrl"),
                    System.getenv("RCS_OPENTCS_BASE_URL")
                ).isPresent(),
                firstNonBlank(
                    System.getProperty("rcs.openTcs.baseUrl"),
                    System.getenv("RCS_OPENTCS_BASE_URL")
                ).orElse(null)
            )
        );
        post(
            "/demo/wcs/callback",
            WcsDemoHttpHandlers.callbackReceiverHandler(demoCallbackRecords)
        );
        post(
            "/demo/wms/inbound-results",
            WcsDemoHttpHandlers.callbackReceiverHandler(demoCallbackRecords)
        );
        post(
            "/demo/wms/outbound-results",
            WcsDemoHttpHandlers.callbackReceiverHandler(demoCallbackRecords)
        );
        post(
            "/api/v1/wms/inbound-results",
            WcsDemoHttpHandlers.callbackReceiverHandler(demoCallbackRecords)
        );
        post(
            "/api/v1/wms/outbound-results",
            WcsDemoHttpHandlers.callbackReceiverHandler(demoCallbackRecords)
        );
        get(
            "/demo/wcs/callbacks",
            WcsDemoHttpHandlers.listCallbacksHandler(demoCallbackRecords)
        );
        post(
            "/demo/wcs/callbacks/clear",
            WcsDemoHttpHandlers.clearCallbacksHandler(demoCallbackRecords)
        );
        post(
            "/api/v1/wcs/agv/missions",
            WcsMissionHttpHandlers.createMissionHandler(missionService, objectMapper)
        );
        get(
            "/api/v1/wcs/agv/missions",
            WcsMissionHttpHandlers.listMissionsHandler(missionService)
        );
        post(
            "/api/v1/wcs/agv/missions/{mission_no}/cancel",
            WcsMissionHttpHandlers.cancelMissionHandler(missionService)
        );
        get(
            "/api/v1/wcs/agv/missions/{mission_no}",
            WcsMissionHttpHandlers.queryMissionHandler(missionService)
        );
        get(
            "/api/v1/wcs/agv/missions/{mission_no}/commands",
            AgvCommandHttpHandlers.queryMissionCommandsHandler(agvCommandOutboxStore)
        );
        post(
            "/api/v1/wcs/inbound/tasks",
            WcsTaskHttpHandlers.createInboundTaskHandler(taskService, objectMapper)
        );
        post(
            "/api/v1/wcs/outbound/tasks",
            WcsTaskHttpHandlers.createOutboundTaskHandler(taskService, objectMapper)
        );
        get(
            "/api/v1/wcs/tasks/{biz_task_no}",
            WcsTaskHttpHandlers.queryTaskHandler(taskService)
        );
        post(
            "/api/v1/wcs/tasks/{biz_task_no}/cancel",
            WcsTaskHttpHandlers.cancelTaskHandler(taskService)
        );
        post(
            "/api/v1/opentcs/events/transport-orders",
            OpenTcsEventHttpHandlers.transportOrderEventHandler(
                ssePayloadParser,
                openTcsEventConsumer,
                callbackRetryProcessor,
                dispatchBatchSize
            )
        );
      });
      cfg.routes.exception(
          IdempotencyConflictException.class,
          (exc, ctx) -> ctx.status(409).json(ApiResponse.error("RCS-4009", exc.getMessage()))
      );
      cfg.routes.exception(
          ResourceNotFoundException.class,
          (exc, ctx) -> ctx.status(404).json(ApiResponse.error("RCS-4004", exc.getMessage()))
      );
      cfg.routes.exception(
          TaskStateConflictException.class,
          (exc, ctx) -> ctx.status(409).json(ApiResponse.error("RCS-4090", exc.getMessage()))
      );
      cfg.routes.exception(
          IllegalArgumentException.class,
          (exc, ctx) -> ctx.status(400).json(ApiResponse.error("RCS-4001", exc.getMessage()))
      );
      cfg.routes.exception(
          OpenTcsClientException.class,
          (exc, ctx) -> ctx.status(502).json(ApiResponse.error("RCS-5002", exc.getMessage()))
      );
      cfg.routes.exception(
          Exception.class,
          (exc, ctx) -> ctx.status(500).json(ApiResponse.error("RCS-5000", "Internal server error"))
      );
    };
    return new AppRuntime(
        Javalin.create(config),
        sseSubscriber,
        agvMqttSubscriber,
        agvCommandRuntime.scheduler(),
        callbackRetryScheduler
    );
  }

  private static MissionStore createMissionStore(ObjectMapper objectMapper) {
    return switch (resolveStoreMode()) {
      case "memory" -> new InMemoryMissionStore();
      case "file" -> new FileMissionStore(resolveStoreDirectory().resolve("mission-store.json"), objectMapper);
      default -> throw invalidStoreModeException();
    };
  }

  private static IdempotencyStore createIdempotencyStore(ObjectMapper objectMapper) {
    return switch (resolveStoreMode()) {
      case "memory" -> new InMemoryIdempotencyStore();
      case "file" -> new FileIdempotencyStore(
          resolveStoreDirectory().resolve("idempotency-store.json"),
          objectMapper
      );
      default -> throw invalidStoreModeException();
    };
  }

  private static TaskStore createTaskStore(ObjectMapper objectMapper) {
    return switch (resolveStoreMode()) {
      case "memory" -> new InMemoryTaskStore();
      case "file" -> new FileTaskStore(resolveStoreDirectory().resolve("task-store.json"), objectMapper);
      default -> throw invalidStoreModeException();
    };
  }

  private static CallbackOutboxStore createCallbackOutboxStore(ObjectMapper objectMapper) {
    return switch (resolveStoreMode()) {
      case "memory" -> new InMemoryCallbackOutboxStore();
      case "file" -> new FileCallbackOutboxStore(
          resolveStoreDirectory().resolve("callback-outbox-store.json"),
          objectMapper
      );
      default -> throw invalidStoreModeException();
    };
  }

  private static AgvCommandOutboxStore createAgvCommandOutboxStore(ObjectMapper objectMapper) {
    return switch (resolveStoreMode()) {
      case "memory" -> new InMemoryAgvCommandOutboxStore();
      case "file" -> new FileAgvCommandOutboxStore(
          resolveStoreDirectory().resolve("agv-command-outbox-store.json"),
          objectMapper
      );
      default -> throw invalidStoreModeException();
    };
  }

  private static int resolvePort(String[] args) {
    if (args != null && args.length > 0) {
      return Integer.parseInt(args[0]);
    }
    return 8080;
  }

  private static OpenTcsOrderClient createOpenTcsOrderClient(ObjectMapper objectMapper) {
    Optional<String> baseUrl = firstNonBlank(
        System.getProperty("rcs.openTcs.baseUrl"),
        System.getenv("RCS_OPENTCS_BASE_URL")
    );
    if (baseUrl.isEmpty()) {
      return new InMemoryOpenTcsOrderClient();
    }
    return new HttpOpenTcsOrderClient(
        HttpClient.newHttpClient(),
        objectMapper,
        URI.create(baseUrl.orElseThrow()),
        Duration.ofMillis(resolveLong(
            "rcs.openTcs.timeoutMillis",
            "RCS_OPENTCS_TIMEOUT_MILLIS",
            3000L
        )),
        resolveInt("rcs.openTcs.maxAttempts", "RCS_OPENTCS_MAX_ATTEMPTS", 3),
        Duration.ofMillis(resolveLong(
            "rcs.openTcs.initialRetryDelayMillis",
            "RCS_OPENTCS_INITIAL_RETRY_DELAY_MILLIS",
            200L
        )),
        firstNonBlank(
            System.getProperty("rcs.openTcs.token"),
            System.getenv("RCS_OPENTCS_TOKEN")
        ).orElse(null)
    );
  }

  private static CallbackSender createCallbackSender() {
    Optional<String> callbackBaseUrl = firstNonBlank(
        System.getProperty("rcs.callback.baseUrl"),
        System.getenv("RCS_CALLBACK_BASE_URL")
    );
    return new HttpCallbackSender(
        HttpClient.newHttpClient(),
        callbackBaseUrl.map(URI::create).orElse(null),
        Duration.ofMillis(resolveLong(
            "rcs.callback.timeoutMillis",
            "RCS_CALLBACK_TIMEOUT_MILLIS",
            3000L
        )),
        firstNonBlank(
            System.getProperty("rcs.callback.token"),
            System.getenv("RCS_CALLBACK_TOKEN")
        ).orElse(null),
        firstNonBlank(
            System.getProperty("rcs.callback.signingSecret"),
            System.getenv("RCS_CALLBACK_SIGNING_SECRET")
        ).orElse(null)
    );
  }

  private static Optional<OpenTcsSseTransportOrderSubscriber> createOpenTcsSseSubscriber(
      OpenTcsSsePayloadParser payloadParser,
      OpenTcsSseEventConsumer eventConsumer,
      CallbackRetryProcessor callbackRetryProcessor,
      int dispatchBatchSize
  ) {
    if (!resolveBoolean("rcs.openTcs.sse.enabled", "RCS_OPENTCS_SSE_ENABLED", false)) {
      return Optional.empty();
    }
    Optional<String> sseUrl = firstNonBlank(
        System.getProperty("rcs.openTcs.sse.url"),
        System.getenv("RCS_OPENTCS_SSE_URL")
    );
    URI sseUri = sseUrl.map(URI::create).orElseGet(RcsIntegrationApplication::buildDefaultSseUri);
    OpenTcsSseTransportOrderSubscriber subscriber = new OpenTcsSseTransportOrderSubscriber(
        HttpClient.newHttpClient(),
        sseUri,
        Duration.ofMillis(resolveLong(
            "rcs.openTcs.sse.requestTimeoutMillis",
            "RCS_OPENTCS_SSE_REQUEST_TIMEOUT_MILLIS",
            600000L
        )),
        Duration.ofMillis(resolveLong(
            "rcs.openTcs.sse.reconnectInitialDelayMillis",
            "RCS_OPENTCS_SSE_RECONNECT_INITIAL_DELAY_MILLIS",
            1000L
        )),
        Duration.ofMillis(resolveLong(
            "rcs.openTcs.sse.reconnectMaxDelayMillis",
            "RCS_OPENTCS_SSE_RECONNECT_MAX_DELAY_MILLIS",
            30000L
        )),
        firstNonBlank(
            System.getProperty("rcs.openTcs.apiAccessKey"),
            System.getenv("RCS_OPENTCS_API_ACCESS_KEY")
        ).orElse(null),
        firstNonBlank(
            System.getProperty("rcs.openTcs.token"),
            System.getenv("RCS_OPENTCS_TOKEN")
        ).orElse(null),
        payloadParser,
        eventConsumer,
        callbackRetryProcessor,
        dispatchBatchSize
    );
    return Optional.of(subscriber);
  }

  private static AgvCommandRuntime createAgvCommandRuntime(
      ObjectMapper objectMapper,
      AgvCommandOutboxStore commandOutboxStore
  ) {
    if (!resolveBoolean("rcs.agvCommand.enabled", "RCS_AGV_COMMAND_ENABLED", false)) {
      return new AgvCommandRuntime(AgvCommandPublisher.noop(), Optional.empty());
    }
    MqttAgvRobotControlPublisher mqttPublisher = createMqttAgvRobotControlPublisher(objectMapper);
    AgvCommandRetryProcessor retryProcessor = new AgvCommandRetryProcessor(
        commandOutboxStore,
        mqttPublisher
    );
    AgvCommandRetryScheduler retryScheduler = new AgvCommandRetryScheduler(
        retryProcessor,
        resolveInt("rcs.agvCommand.dispatchBatchSize", "RCS_AGV_COMMAND_DISPATCH_BATCH_SIZE", 100),
        Duration.ofMillis(resolveLong(
            "rcs.agvCommand.retryTickMillis",
            "RCS_AGV_COMMAND_RETRY_TICK_MILLIS",
            1000L
        ))
    );
    return new AgvCommandRuntime(
        new AgvCommandOutboxService(commandOutboxStore, mqttPublisher),
        Optional.of(retryScheduler)
    );
  }

  private static MqttAgvRobotControlPublisher createMqttAgvRobotControlPublisher(ObjectMapper objectMapper) {
    Map<String, Integer> pointIdMap = parsePointIdMap(
        firstNonBlank(
            System.getProperty("rcs.agvCommand.pointIdMap"),
            System.getenv("RCS_AGV_POINT_ID_MAP")
        ).orElseThrow(
            () -> new IllegalArgumentException(
                "AGV command publishing requires rcs.agvCommand.pointIdMap or RCS_AGV_POINT_ID_MAP"
            )
        )
    );
    return new MqttAgvRobotControlPublisher(
        firstNonBlank(
            System.getProperty("rcs.agvCommand.brokerUri"),
            System.getenv("RCS_AGV_COMMAND_BROKER_URI")
        ).orElse("tcp://127.0.0.1:1883"),
        firstNonBlank(
            System.getProperty("rcs.agvCommand.clientId"),
            System.getenv("RCS_AGV_COMMAND_CLIENT_ID")
        ).orElse("rcs-agv-command-publisher"),
        firstNonBlank(
            System.getProperty("rcs.agvCommand.topic"),
            System.getenv("RCS_AGV_COMMAND_TOPIC")
        ).orElse("robot_control"),
        resolveInt("rcs.agvCommand.qos", "RCS_AGV_COMMAND_QOS", 1),
        firstNonBlank(
            System.getProperty("rcs.agvCommand.username"),
            System.getenv("RCS_AGV_COMMAND_USERNAME")
        ).orElse(null),
        firstNonBlank(
            System.getProperty("rcs.agvCommand.password"),
            System.getenv("RCS_AGV_COMMAND_PASSWORD")
        ).orElse(null),
        pointIdMap,
        resolveDouble("rcs.agvCommand.defaultRunSpeed", "RCS_AGV_DEFAULT_RUN_SPEED", 0.5),
        objectMapper
    );
  }

  private static Optional<AgvMqttStatusSubscriber> createAgvMqttSubscriber(
      ObjectMapper objectMapper,
      CallbackOutboxService callbackOutboxService,
      MissionStore missionStore,
      TaskStore taskStore,
      WmsTaskResultService wmsTaskResultService,
      CallbackRetryProcessor callbackRetryProcessor,
      int dispatchBatchSize
  ) {
    if (!resolveBoolean("rcs.agvMqtt.enabled", "RCS_AGV_MQTT_ENABLED", false)) {
      return Optional.empty();
    }
    AgvMqttStatusSubscriber subscriber = new AgvMqttStatusSubscriber(
        firstNonBlank(
            System.getProperty("rcs.agvMqtt.brokerUri"),
            System.getenv("RCS_AGV_MQTT_BROKER_URI")
        ).orElse("tcp://127.0.0.1:1883"),
        firstNonBlank(
            System.getProperty("rcs.agvMqtt.clientId"),
            System.getenv("RCS_AGV_MQTT_CLIENT_ID")
        ).orElse("rcs-agv-status-subscriber"),
        firstNonBlank(
            System.getProperty("rcs.agvMqtt.topic"),
            System.getenv("RCS_AGV_MQTT_TOPIC")
        ).orElse("agv/+/#"),
        resolveInt("rcs.agvMqtt.qos", "RCS_AGV_MQTT_QOS", 1),
        firstNonBlank(
            System.getProperty("rcs.agvMqtt.username"),
            System.getenv("RCS_AGV_MQTT_USERNAME")
        ).orElse(null),
        firstNonBlank(
            System.getProperty("rcs.agvMqtt.password"),
            System.getenv("RCS_AGV_MQTT_PASSWORD")
        ).orElse(null),
        new AgvMqttStatusPayloadParser(objectMapper),
        new AgvMqttStatusEventConsumer(
            callbackOutboxService,
            missionStore,
            taskStore,
            wmsTaskResultService
        ),
        callbackRetryProcessor,
        dispatchBatchSize
    );
    return Optional.of(subscriber);
  }

  private static Optional<String> resolveWmsBaseUrl() {
    return firstNonBlank(
        System.getProperty("rcs.wms.baseUrl"),
        System.getenv("RCS_WMS_BASE_URL")
    );
  }

  private static URI buildDefaultSseUri() {
    String baseUrl = firstNonBlank(
        System.getProperty("rcs.openTcs.baseUrl"),
        System.getenv("RCS_OPENTCS_BASE_URL")
    ).orElseThrow(
        () -> new IllegalStateException(
            "openTCS SSE requires rcs.openTcs.sse.url or rcs.openTcs.baseUrl"
        )
    );
    String normalizedBase = baseUrl.endsWith("/") ? baseUrl : baseUrl + "/";
    return URI.create(normalizedBase + "v1/sse?/events/transportOrders=true");
  }

  private static int resolveInt(String propertyName, String envName, int defaultValue) {
    return firstNonBlank(System.getProperty(propertyName), System.getenv(envName))
        .map(Integer::parseInt)
        .orElse(defaultValue);
  }

  private static double resolveDouble(String propertyName, String envName, double defaultValue) {
    return firstNonBlank(System.getProperty(propertyName), System.getenv(envName))
        .map(Double::parseDouble)
        .orElse(defaultValue);
  }

  private static Map<String, Integer> parsePointIdMap(String value) {
    Map<String, Integer> result = new LinkedHashMap<>();
    for (String entry : value.split(",")) {
      if (entry.isBlank()) {
        continue;
      }
      String[] parts = entry.trim().split("[:=]", 2);
      if (parts.length != 2 || parts[0].isBlank() || parts[1].isBlank()) {
        throw new IllegalArgumentException("Invalid AGV point id map entry: " + entry);
      }
      result.put(parts[0].trim(), Integer.parseInt(parts[1].trim()));
    }
    if (result.isEmpty()) {
      throw new IllegalArgumentException("AGV point id map must not be empty");
    }
    return result;
  }

  private static long resolveLong(String propertyName, String envName, long defaultValue) {
    return firstNonBlank(System.getProperty(propertyName), System.getenv(envName))
        .map(Long::parseLong)
        .orElse(defaultValue);
  }

  private static boolean resolveBoolean(String propertyName, String envName, boolean defaultValue) {
    return firstNonBlank(System.getProperty(propertyName), System.getenv(envName))
        .map(Boolean::parseBoolean)
        .orElse(defaultValue);
  }

  private static Optional<String> firstNonBlank(String first, String second) {
    if (first != null && !first.isBlank()) {
      return Optional.of(first.trim());
    }
    if (second != null && !second.isBlank()) {
      return Optional.of(second.trim());
    }
    return Optional.empty();
  }

  private static String resolveStoreMode() {
    return firstNonBlank(
        System.getProperty("rcs.store.mode"),
        System.getenv("RCS_STORE_MODE")
    ).orElse("memory").trim().toLowerCase(Locale.ROOT);
  }

  private static Path resolveStoreDirectory() {
    return firstNonBlank(
        System.getProperty("rcs.store.file.dir"),
        System.getenv("RCS_STORE_FILE_DIR")
    ).map(Paths::get)
        .orElseGet(() -> Paths.get(".rcs-store"))
        .toAbsolutePath();
  }

  private static IllegalArgumentException invalidStoreModeException() {
    return new IllegalArgumentException(
        "Unsupported store mode. Expected one of [memory, file], but got: " + resolveStoreMode()
    );
  }

  private record AgvCommandRuntime(
      AgvCommandPublisher publisher,
      Optional<AgvCommandRetryScheduler> scheduler
  ) {

    private AgvCommandRuntime {
      Objects.requireNonNull(publisher, "publisher");
      Objects.requireNonNull(scheduler, "scheduler");
    }
  }

  private record AppRuntime(
      Javalin app,
      Optional<OpenTcsSseTransportOrderSubscriber> sseSubscriber,
      Optional<AgvMqttStatusSubscriber> agvMqttSubscriber,
      Optional<AgvCommandRetryScheduler> agvCommandRetryScheduler,
      CallbackRetryScheduler callbackRetryScheduler
  ) {

    private AppRuntime {
      Objects.requireNonNull(app, "app");
      Objects.requireNonNull(sseSubscriber, "sseSubscriber");
      Objects.requireNonNull(agvMqttSubscriber, "agvMqttSubscriber");
      Objects.requireNonNull(agvCommandRetryScheduler, "agvCommandRetryScheduler");
      Objects.requireNonNull(callbackRetryScheduler, "callbackRetryScheduler");
    }

    private void startBackgroundWorkers() {
      callbackRetryScheduler.start();
      agvCommandRetryScheduler.ifPresent(AgvCommandRetryScheduler::start);
      sseSubscriber.ifPresent(OpenTcsSseTransportOrderSubscriber::start);
      agvMqttSubscriber.ifPresent(AgvMqttStatusSubscriber::start);
    }

    private void stopBackgroundWorkers() {
      agvMqttSubscriber.ifPresent(AgvMqttStatusSubscriber::stop);
      sseSubscriber.ifPresent(OpenTcsSseTransportOrderSubscriber::stop);
      agvCommandRetryScheduler.ifPresent(AgvCommandRetryScheduler::stop);
      callbackRetryScheduler.stop();
    }
  }
}
