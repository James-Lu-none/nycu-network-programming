#include <DHT11.h>
#include <WiFiS3.h>
#include <stdio.h>
#include <stdlib.h>
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>
#include "secrets.h"

ArduinoLEDMatrix matrix;

#define DHT11PIN 2
DHT11 dht11(DHT11PIN);

char wifi_ssid[] = WIFI_SSID;
char wifi_pass[] = WIFI_PASSWORD;
int wifi_status = WL_IDLE_STATUS;

// Server connection settings
const char* serverHost = "nycu.waynewolf.tw";
const int serverPort = 80;
const char* serverPath = "/weather/weather.do";

// Cached server weather data
String serverTime = "";
String serverTemp = "";
String serverHumid = "";
bool hasServerData = false;

// Cached local sensor data
int cachedTemperature = 0;
int cachedHumidity = 0;
bool lastReadSuccessful = false;

// Scheduling timers
unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 30000; // Request every 30 seconds

void print_wifi_status() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI): ");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize LED matrix
  matrix.begin();

  // Check for the WiFi module
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  // Attempt to connect to WiFi network
  Serial.print("Connecting to SSID: ");
  Serial.println(wifi_ssid);

  wifi_status = WiFi.begin(wifi_ssid, wifi_pass);
  while (wifi_status != WL_CONNECTED) {
    delay(2000);
    Serial.print(".");
    wifi_status = WiFi.status();
  }

  Serial.println("\nConnected to WiFi!");
  print_wifi_status();
}

void drawClock() {
  matrix.beginDraw();
  matrix.clear();
  matrix.stroke(0xFFFFFF);
  
  // Octagon outline for clock
  matrix.line(4, 1, 7, 1);
  matrix.line(4, 6, 7, 6);
  matrix.line(3, 2, 3, 5);
  matrix.line(8, 2, 8, 5);
  
  // Center and hands
  matrix.point(5, 3);
  matrix.point(5, 2); // Hour hand pointing up
  matrix.point(6, 3); // Minute hand pointing right
  
  matrix.endDraw();
}

void drawThermometer() {
  matrix.beginDraw();
  matrix.clear();
  matrix.stroke(0xFFFFFF);
  
  // Stem outline
  matrix.line(5, 1, 5, 3);
  matrix.line(7, 1, 7, 3);
  matrix.point(6, 0); // Round top cap
  
  // Bulb outline at bottom
  matrix.point(5, 4);
  matrix.point(7, 4);
  matrix.point(4, 5);
  matrix.point(8, 5);
  matrix.point(4, 6);
  matrix.point(8, 6);
  matrix.line(5, 7, 7, 7);
  
  // Mercury line inside
  matrix.line(6, 2, 6, 6);
  
  matrix.endDraw();
}

void drawWaterDrop() {
  matrix.beginDraw();
  matrix.clear();
  matrix.stroke(0xFFFFFF);
  
  // Outline of water droplet
  matrix.point(6, 0);
  matrix.point(5, 1);
  matrix.point(7, 1);
  matrix.point(4, 2);
  matrix.point(8, 2);
  matrix.point(3, 3);
  matrix.point(9, 3);
  matrix.line(2, 4, 2, 5);
  matrix.line(10, 4, 10, 5);
  matrix.point(3, 6);
  matrix.point(9, 6);
  matrix.line(4, 7, 8, 7);
  
  matrix.endDraw();
}

void scrollText(String text) {
  int textLength = text.length();
  int endX = - (textLength * 5); // 4px char width + 1px spacing
  for (int x = 12; x >= endX; x--) {
    matrix.beginDraw();
    matrix.clear();
    matrix.textFont(Font_4x6);
    matrix.beginText(x, 1, 0xFFFFFF);
    matrix.print(text);
    matrix.endText();
    matrix.endDraw();
    delay(70); // Scroll speed control
  }
}

void updateWeatherData() {
  // Reconnect WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Reconnecting...");
    WiFi.disconnect();
    wifi_status = WiFi.begin(wifi_ssid, wifi_pass);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10) {
      delay(1000);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnection failed.");
      return;
    }
  }

  WiFiClient client;
  Serial.println("Connecting to server...");
  if (client.connect(serverHost, serverPort)) {
    Serial.println("Connected! Sending POST request...");
    
    // x-www-form-urlencoded payload
    String postData = "time=now&location=taipei";
    
    client.println("POST " + String(serverPath) + " HTTP/1.1");
    client.println("Host: " + String(serverHost));
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(postData.length());
    client.println("Connection: close");
    client.println();
    client.print(postData);
    client.println();
    
    // Wait for response
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 10000) {
        Serial.println(">>> HTTP Client Timeout!");
        client.stop();
        return;
      }
    }
    
    // Read and parse
    bool isBody = false;
    String body = "";
    while (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      if (!isBody) {
        if (line.length() == 0) {
          isBody = true;
        }
      } else {
        if (line.length() > 0) {
          body += line;
        }
      }
    }
    client.stop();
    
    body.trim();
    Serial.print("Server Response Body: ");
    Serial.println(body);
    
    // Parse: asktime,temp,humid
    int comma1 = body.indexOf(',');
    int comma2 = body.indexOf(',', comma1 + 1);
    if (comma1 != -1 && comma2 != -1) {
      serverTime = body.substring(0, comma1);
      serverTemp = body.substring(comma1 + 1, comma2);
      serverHumid = body.substring(comma2 + 1);
      
      serverTime.trim();
      serverTemp.trim();
      serverHumid.trim();
      
      hasServerData = true;
      Serial.println("Successfully updated weather data from server.");
    } else {
      Serial.println("Parsing error: response format invalid.");
    }
  } else {
    Serial.println("Connection to server failed.");
  }
}

void loop() {
  unsigned long currentMillis = millis();
  
  // 1. Update server weather data if interval has passed or first request
  if (lastRequestTime == 0 || currentMillis - lastRequestTime >= requestInterval) {
    lastRequestTime = currentMillis;
    updateWeatherData();
  }
  
  // 2. Read local sensor
  int temp = 0;
  int hum = 0;
  int result = dht11.readTemperatureHumidity(temp, hum);
  if (result == 0) {
    cachedTemperature = temp;
    cachedHumidity = hum;
    lastReadSuccessful = true;
    Serial.print("Local Temp: "); Serial.print(cachedTemperature);
    Serial.print(", Local Humid: "); Serial.println(cachedHumidity);
  } else {
    lastReadSuccessful = false;
    Serial.println(DHT11::getErrorString(result));
  }
  
  // 3. Display cycle
  if (hasServerData) {
    // --- Display Server Time ---
    drawClock();
    delay(1500);
    scrollText("Time: " + serverTime);
    
    // --- Display Server Temp ---
    drawThermometer();
    delay(1500);
    scrollText("Taipei: " + serverTemp + "C");
    
    // --- Display Server Humid ---
    drawWaterDrop();
    delay(1500);
    scrollText("Taipei: " + serverHumid + "%");
  } else {
    scrollText("No Server Data");
    delay(1000);
  }
  
  if (lastReadSuccessful) {
    // --- Display Local Temp ---
    drawThermometer();
    delay(1500);
    scrollText("Local: " + String(cachedTemperature) + "C");
    
    // --- Display Local Humid ---
    drawWaterDrop();
    delay(1500);
    scrollText("Local: " + String(cachedHumidity) + "%");
  } else {
    delay(1500);
  }
}