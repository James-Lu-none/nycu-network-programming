# Weather Service Client

## design decisions

1. Sequential Polling & Rotation:
    - query weather data for Taipei, Taoyuan, Hsinchu, and Miaoli sequentially.
    - send HTTP request every 3 seconds (3000ms interval) to rotatively fetch the latest data for each city.
2. Non-blocking HTTP client state machine:
    - use two states `HTTP_IDLE` and `HTTP_AWAITING_RESPONSE` to process requests asynchronously.
    - check `client.available()` without blocking the loop, keeping the LED matrix scroll animation smooth.
    - request timeout is set to 3000ms. If server doesn't respond in time, clean up connection and mark target city data as unavailable.
3. DNS resolution caching:
    - resolve `nycu.waynewolf.tw` to `serverIP` once using `WiFi.hostByName()` during startup or first request.
    - use resolved IP address directly for future socket connections to avoid blocking DNS query overhead.
4. TCP connection reuse:
    - send `Connection: keep-alive` header in POST requests.
    - check `client.connected()` before initiating connection, if still connected, reuse the socket for subsequent requests to avoid TCP handshake delay.
5. LED matrix UI & indicator dots:
    - format and scroll text string like `"{city}: {time} Temp:{temp}C Humid:{humid}%"` on matrix.
    - use bottom right 4 dots (columns 8 to 11 on row 7) as status indicators for Taipei, Taoyuan, Hsinchu, and Miaoli respectively.
    - if a location is currently fetching data, its corresponding dot will blink.
    - if a location has cached data, its dot stays lit. Otherwise, the dot is off.
6. resilient wifi reconnection:
    - check wifi connection status periodically. If disconnected, attempt to reconnect every 15 seconds without blocking the display scroll.

## my own creativity

1. Non-blocking asynchronous network state machine ensures the LED matrix animation never lags.
2. DNS IP caching prevents blocking operations in main loop caused by periodic DNS lookup.
3. Multi-location request/status visualization using blinking/steady indicator dots on the bottom row of the LED matrix.
