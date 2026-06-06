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

// Server settings
const char* serverHost = "172.31.0.102"; // IP of the computer running http_server.c
const int serverPort = 8080;

unsigned long lastSensorReadTime = 0;
unsigned long lastDisplaySwitchTime = 0;
unsigned long lastPostTime = 0;
const unsigned long postInterval = 10000; // 10 seconds

int currentDisplayState = 0; // 0 = temperature, 1 = humidity
int cachedTemperature = 0;
int cachedHumidity = 0;
bool lastReadSuccessful = false;

void print_wifi_status() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  
  long rssi = WiFi.RSSI();
  Serial.print("Signal (RSSI): ");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  matrix.begin();

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

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

void showTemperature(int temp) {
  matrix.beginDraw();
  matrix.clear();
  
  int x = (temp >= 10 || temp < 0 || temp <= -10) ? 0 : 4;
  
  matrix.textFont(Font_4x6);
  matrix.beginText(x, 1, 0xFFFFFF);
  matrix.print(temp);
  matrix.endText();
  
  matrix.stroke(0xFFFFFF);
  matrix.point(8, 1);
  matrix.point(11, 2);
  matrix.point(10, 2);
  matrix.point(9, 3);
  matrix.point(9, 4);
  matrix.point(10, 5);
  matrix.point(11, 5);
  
  matrix.endDraw();
}

void showHumidity(int hum) {
  matrix.beginDraw();
  matrix.clear();
  
  int x = (hum >= 10 || hum < 0) ? 0 : 4;
  
  matrix.textFont(Font_4x6);
  matrix.beginText(x, 1, 0xFFFFFF);
  matrix.print(hum);
  matrix.endText();
  
  matrix.stroke(0xFFFFFF);
  matrix.point(8, 2);
  matrix.point(11, 2);
  matrix.point(10, 3);
  matrix.point(9, 4);
  matrix.point(8, 5);
  matrix.point(11, 5);
  matrix.endDraw();
}

void showError() {
  matrix.beginDraw();
  matrix.clear();
  matrix.stroke(0xFFFFFF);
  matrix.line(0, 1, 0, 6);
  matrix.line(0, 1, 2, 1);
  matrix.line(0, 3, 2, 3);
  matrix.line(0, 6, 2, 6);
  matrix.endDraw();
}

void postWeatherData(int temp, int hum) {
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
  Serial.print("Connecting to weather server: ");
  Serial.print(serverHost);
  Serial.print(":");
  Serial.println(serverPort);
  
  if (client.connect(serverHost, serverPort)) {
    Serial.println("Connected! Sending POST request...");
    
    // Format: <temperature>, <humidity>
    String postBody = String(temp) + ", " + String(hum);
    
    client.println("POST / HTTP/1.1");
    client.print("Host: ");
    client.println(serverHost);
    client.println("Content-Type: text/plain");
    client.print("Content-Length: ");
    client.println(postBody.length());
    client.println("Connection: close");
    client.println();
    client.print(postBody);
    client.println();
    
    // Wait for response
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> HTTP Post Timeout!");
        client.stop();
        return;
      }
    }
    
    Serial.println("Response:");
    while (client.available()) {
      char c = client.read();
      Serial.write(c);
    }
    client.stop();
    Serial.println("\nPOST completed.");
  } else {
    Serial.println("Connection to server failed.");
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Read local sensor every 100 ms
  if (lastSensorReadTime == 0 || currentMillis - lastSensorReadTime >= 100) {
    lastSensorReadTime = currentMillis;
    int temp = 0;
    int hum = 0;
    int result = dht11.readTemperatureHumidity(temp, hum);
    if (result == 0) {
      cachedTemperature = temp;
      cachedHumidity = hum;
      lastReadSuccessful = true;
    } else {
      lastReadSuccessful = false;
      Serial.println(DHT11::getErrorString(result));
    }
  }

  // 2. Display temperature and humidity alternately every 2 seconds
  if (lastDisplaySwitchTime == 0 || currentMillis - lastDisplaySwitchTime >= 2000) {
    lastDisplaySwitchTime = currentMillis;
    if (lastReadSuccessful) {
      if (currentDisplayState == 0) {
        showTemperature(cachedTemperature);
        currentDisplayState = 1;
      } else {
        showHumidity(cachedHumidity);
        currentDisplayState = 0;
      }
    } else {
      showError();
    }
  }

  // 3. Post data to the server every 10 seconds
  if (lastPostTime == 0 || currentMillis - lastPostTime >= postInterval) {
    lastPostTime = currentMillis;
    if (lastReadSuccessful) {
      postWeatherData(cachedTemperature, cachedHumidity);
    } else {
      Serial.println("Skipping POST request: DHT11 sensor read failed.");
    }
  }
}