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

const char* serverHost = "192.168.0.101";
const int serverPort = 8080;

struct WeatherData {
  int temp;
  int humid;
  bool hasData;
};

WeatherData localData = {0, 0, false};

enum UploadResult {
  RESULT_NONE,
  RESULT_SUCCESS,
  RESULT_DUPLICATE,
  RESULT_ERROR
};
UploadResult lastUploadResult = RESULT_NONE;

unsigned long lastSensorReadTime = 0;
unsigned long lastPostTime = 0;
const unsigned long postInterval = 10000;

WiFiClient client;
enum HttpState {
  HTTP_IDLE,
  HTTP_AWAITING_RESPONSE
};
HttpState httpState = HTTP_IDLE;
unsigned long requestSendTime = 0;
const unsigned long responseTimeout = 3000;
unsigned long lastConnectAttempt = 0;

unsigned long lastDisplayStateTime = 0;
int scrollX = 12;
int scrollEndX = 0;
String scrollTextStr = "";

void updateScrollFrame() {
  matrix.beginDraw();
  matrix.clear();
  matrix.textFont(Font_4x6);
  matrix.beginText(scrollX, 1, 0xFFFFFF);
  matrix.print(scrollTextStr);
  matrix.endText();
  
  // Status indicator dots at the bottom-right corner (row 7)
  matrix.stroke(0xFFFFFF);
  if (lastUploadResult == RESULT_SUCCESS) {
    matrix.point(9, 7);
  } else if (lastUploadResult == RESULT_DUPLICATE) {
    matrix.point(10, 7);
  } else if (lastUploadResult == RESULT_ERROR) {
    matrix.point(11, 7);
  }

  matrix.endDraw();
}

void setupSlide() {
  scrollTextStr = localData.hasData 
    ? "Local Temp:" + String(localData.temp) + "C Humid:" + String(localData.humid) + "%" 
    : "Local: No data";
  scrollEndX = - (int)(scrollTextStr.length() * 5);
  scrollX = 12;
  updateScrollFrame();
}

void handleDisplay() {
  unsigned long now = millis();
  if (now - lastDisplayStateTime >= 70) {
    scrollX--;
    if (scrollX >= scrollEndX) {
      updateScrollFrame();
    } else {
      setupSlide();
    }
    lastDisplayStateTime = now;
  }
}

void sendPostRequest() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastConnectAttempt >= 15000) {
      WiFi.disconnect();
      WiFi.begin(wifi_ssid, wifi_pass);
      lastConnectAttempt = now;
    }
    lastUploadResult = RESULT_ERROR;
    return;
  }
  if (httpState != HTTP_IDLE) return;
  if (!localData.hasData) return;

  bool isConnected = client.connected();
  if (isConnected || client.connect(serverHost, serverPort)) {
    String postBody = String(localData.temp) + ", " + String(localData.humid);
    client.println("POST / HTTP/1.1");
    client.print("Host: ");
    client.println(serverHost);
    client.println("Content-Type: text/plain");
    client.print("Content-Length: ");
    client.println(postBody.length());
    client.println("Connection: keep-alive");
    client.println();
    client.print(postBody);
    client.println();
    
    httpState = HTTP_AWAITING_RESPONSE;
    requestSendTime = millis();
  } else {
    lastUploadResult = RESULT_ERROR;
  }
}

void handleResponse() {
  if (httpState != HTTP_AWAITING_RESPONSE) return;

  if (client.available() > 0) {
    bool isBody = false;
    String body = "";
    while (client.available() > 0) {
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
    httpState = HTTP_IDLE;
    body.trim();
    if (body.indexOf("Duplicate") != -1) {
      lastUploadResult = RESULT_DUPLICATE;
    } else if (body.indexOf("success") != -1 || body.indexOf("received") != -1) {
      lastUploadResult = RESULT_SUCCESS;
    } else {
      lastUploadResult = RESULT_ERROR;
    }
  } else if (!client.connected() || (millis() - requestSendTime > responseTimeout)) {
    client.stop();
    httpState = HTTP_IDLE;
    lastUploadResult = RESULT_ERROR;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  matrix.begin();
  while (WiFi.status() == WL_NO_MODULE);

  wifi_status = WiFi.begin(wifi_ssid, wifi_pass);
  while (wifi_status != WL_CONNECTED) {
    delay(2000);
    wifi_status = WiFi.status();
  }
  client.setTimeout(50);
  setupSlide();
  lastDisplayStateTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastPostTime >= postInterval) {
    int temp = 0, hum = 0;
    if (dht11.readTemperatureHumidity(temp, hum) == 0) {
      localData.temp = temp;
      localData.humid = hum;
      localData.hasData = true;
    } else {
      localData.hasData = false;
    }
    sendPostRequest();
    lastPostTime = millis();
  }

  handleDisplay();
  handleResponse();
}