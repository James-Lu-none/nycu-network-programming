# Weather Collector

## design decisions

1. telemetry node (Arduino client):
    - read temperature and humidity from DHT11 sensor (pin 2) every 10 seconds.
    - format the telemetry as plain text payload like `"<temperature>, <humidity>"`.
    - use a non-blocking asynchronous state machine (`HTTP_IDLE` and `HTTP_AWAITING_RESPONSE`) to send HTTP POST to the local server.
    - scroll the local readings on the LED Matrix.
2. cross-platform socket layer (C server):
    - use a single header `sock_compat.h` to abstract socket operations between Windows (Winsock2) and Linux/macOS (POSIX).
3. single-threaded I/O multiplexing:
    - use `select()` in the server main loop to manage the listen socket and active client connections concurrently.
    - maintain a dynamic linked list of clients (`client_info`) to handle partial reads, matching incoming request size with the `Content-Length` header.
4. IPv6 & IPv4 dual-stack support:
    - create socket in `AF_INET6` family and disable `IPV6_V6ONLY` option using `setsockopt()`.
    - allow server to bind to a single port (8080) and accept connections from both IPv4 and IPv6 clients.
5. duplicate data filtering:
    - when a POST request is received, compare the values with the last line in `weather.csv`.
    - if the values are identical, return `200 OK` with "Duplicate data received. Ignoring." and do not write to the file to save disk I/O.

## my own creativity

1. Cross-platform socket wrapper (`sock_compat.h`) allows compiling the exact same C server code on Windows and Linux/macOS.
2. Single-threaded concurrency with `select()` avoids the overhead and potential race conditions of multi-threading.
3. Telemetry deduplication mechanism prevents logging identical data points from stable sensor outputs, optimizing storage writes.
