# Weather Service Client

## design decisions

1. Sequential Polling & Rotation:
    - query weather data for Taipei, Taoyuan, Hsinchu, and Miaoli sequentially.
    - send HTTP request every 3 seconds (3000ms interval) to rotatively fetch the latest data for each city.
    - request timeout is set to 3000ms. If server doesn't respond in time, clean up connection and mark target city data as unavailable.
2. non-blocking state machine on Arduino:
    - all the operations including Wi-Fi connection, HTTP client request, LED matrix scroll are performed in a non-blocking manner with a state machine.
    - use two states `HTTP_IDLE` and `HTTP_AWAITING_RESPONSE` to process requests asynchronously.
    - check `client.available()` without blocking the loop, keeping the LED matrix scroll animation smooth.
3. TCP connection reuse:
    - send `Connection: keep-alive` header in POST requests.
    - check `client.connected()` before initiating connection, if still connected, reuse the socket for subsequent requests to avoid TCP handshake delay.
4. LED matrix UI & indicator dots:
    - format and scroll text string like `"{city}: {time} Temp:{temp}C Humid:{humid}%"` on matrix.
    - use bottom right 4 dots (columns 8 to 11 on row 7) as status indicators for Taipei, Taoyuan, Hsinchu, and Miaoli respectively.
    - if a location is currently fetching data, its corresponding dot will blink.
    - if a location has cached data, its dot stays lit. Otherwise, the dot is off.

## my own creativity

1. Non-blocking asynchronous network state machine ensures the LED matrix animation never lags.
2. Multi-location request/status visualization using blinking/steady indicator dots on the bottom row of the LED matrix.
