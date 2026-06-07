# Weather Collector

## design decisions

1. non-blocking state machine on Arduino:
    - all the operations including Wi-Fi connection, sensor reading, HTTP POST request and LED matrix scroll are performed in a non-blocking manner with a state machine.
2. duplicate data filtering:
    - when a POST request is received, compare the values with the last line in `weather.csv`.
    - if the values are identical, return `200 OK` with "Duplicate data received. Ignoring." and do not write to the file to save disk I/O.

## my own creativity

1. Interactive GET Web Dashboard:
    - Requesting `GET /`, `/index.html`, or `/dashboard` serves a modern glassmorphism dashboard designed by AI.
    - It showcases current temperature/humidity cards and dynamically plots historical weather trends using a dual-axis line chart powered by Chart.js.
2. Three-dot status LED Indicators on arduino:
    - The bottom-right row of the Arduino LED Matrix acts as a status board using single dots:
        - Col 9 (Success): Lit if the last data upload succeeded.
        - Col 10 (Duplicate): Lit if the data upload was ignored due to duplicates.
        - Col 11 (Error/Timeout): Lit if the upload timed out or failed to connect.
