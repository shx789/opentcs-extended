import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpHandler;
import com.sun.net.httpserver.HttpServer;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.concurrent.Executors;

public class WcsMockServer {

  private static final DateTimeFormatter TS = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");

  public static void main(String[] args) throws Exception {
    int port = args.length > 0 ? Integer.parseInt(args[0]) : 18081;
    Path logFile = args.length > 1
        ? Path.of(args[1]).toAbsolutePath()
        : Path.of("build", "demo", "wcs-callbacks.log").toAbsolutePath();
    Files.createDirectories(logFile.getParent());
    Files.writeString(logFile, "", StandardCharsets.UTF_8, StandardOpenOption.CREATE, StandardOpenOption.TRUNCATE_EXISTING);

    HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", port), 0);
    server.setExecutor(Executors.newCachedThreadPool());
    server.createContext("/health", exchange -> writeJson(exchange, 200, "{\"status\":\"ok\"}"));
    server.createContext("/api/v1/wcs/agv/events", new EventHandler(logFile));
    server.start();

    Runtime.getRuntime().addShutdownHook(new Thread(() -> server.stop(0)));
    Thread.currentThread().join();
  }

  private static class EventHandler implements HttpHandler {

    private final Path logFile;

    private EventHandler(Path logFile) {
      this.logFile = logFile;
    }

    @Override
    public void handle(HttpExchange exchange) throws IOException {
      String method = exchange.getRequestMethod();
      if (!"POST".equalsIgnoreCase(method)) {
        writeJson(exchange, 405, "{\"code\":\"405\",\"msg\":\"METHOD_NOT_ALLOWED\"}");
        return;
      }

      String body;
      try (InputStream in = exchange.getRequestBody()) {
        body = new String(in.readAllBytes(), StandardCharsets.UTF_8);
      }

      String line = "[" + TS.format(LocalDateTime.now()) + "] " + body + System.lineSeparator();
      Files.writeString(
          logFile,
          line,
          StandardCharsets.UTF_8,
          StandardOpenOption.CREATE,
          StandardOpenOption.APPEND
      );

      writeJson(
          exchange,
          200,
          "{\"code\":\"0\",\"msg\":\"OK\",\"data\":{\"wcs_status\":\"ACCEPTED\",\"idem_hit\":false}}"
      );
    }
  }

  private static void writeJson(HttpExchange exchange, int status, String json) throws IOException {
    byte[] bytes = json.getBytes(StandardCharsets.UTF_8);
    exchange.getResponseHeaders().set("Content-Type", "application/json; charset=utf-8");
    exchange.sendResponseHeaders(status, bytes.length);
    try (OutputStream out = exchange.getResponseBody()) {
      out.write(bytes);
    }
  }
}
