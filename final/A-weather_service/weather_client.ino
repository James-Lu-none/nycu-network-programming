#include <WiFiS3.h>
#include <stdio.h>
#include <stdlib.h>
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>
#include "secrets.h"

ArduinoLEDMatrix matrix;

char wifi_ssid[] = WIFI_SSID;
char wifi_pass[] = WIFI_PASSWORD;
int wifi_status = WL_IDLE_STATUS;

const char* serverHost = "nycu.waynewolf.tw";
const int serverPort = 80;
const char* serverPath = "/weather/weather.do";

struct WeatherData {
  const char* queryName;
  const char* displayName;
  String time;
  String temp;
  String humid;
  bool hasData;
};

WeatherData data[4] = {
  {"taipei", "Taipei", "", "", "", false},
  {"taoyuan", "Taoyuan", "", "", "", false},
  {"hsinchu", "Hsinchu", "", "", "", false},
  {"miaoli", "Miaoli", "", "", "", false}
};

int fetchLocationIndex = 0;

unsigned long lastRequestTime = 0;
const unsigned long requestInterval = 10000;

WiFiClient client;
enum HttpState {
  HTTP_IDLE,
  HTTP_AWAITING_RESPONSE
};
HttpState httpState = HTTP_IDLE;
unsigned long requestSendTime = 0;
const unsigned long responseTimeout = 5000;

enum SlideState {
  SLIDE_TAIPEI = 0,
  SLIDE_TAOYUAN = 1,
  SLIDE_HSINCHU = 2,
  SLIDE_MIAOLI = 3
};

SlideState currentSlide = SLIDE_TAIPEI;
unsigned long lastDisplayStateTime = 0;
int scrollX = 12;
int scrollEndX = 0;
String scrollTextStr = "";

unsigned long loopCount = 0;

void updateScrollFrame() {
  matrix.beginDraw();
  matrix.clear();
  matrix.textFont(Font_4x6);
  matrix.beginText(scrollX, 1, 0xFFFFFF);
  matrix.print(scrollTextStr);
  matrix.endText();
  
  // Calculate dotX dynamically: moves right by 1 pixel every loop iteration
  int dotX = loopCount % 12;
  matrix.stroke(0xFFFFFF);
  matrix.point(dotX, 7);
  
  matrix.endDraw();
}

void setupSlide() {
  int idx = (int)currentSlide;
  if (data[idx].hasData) {
    scrollTextStr = String(data[idx].displayName) + ": " + data[idx].time + 
                    " Temp:" + data[idx].temp + "C Humid:" + data[idx].humid + "%";
  } else {
    scrollTextStr = String(data[idx].displayName) + ": No data";
  }
  
  int textLength = scrollTextStr.length();
  scrollEndX = - (textLength * 5);
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
      currentSlide = (SlideState)(((int)currentSlide + 1) % 4);
      setupSlide();
    }
    lastDisplayStateTime = now;
  }
}

void sendRequest() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Initiating background reconnection...");
    WiFi.disconnect();
    WiFi.begin(wifi_ssid, wifi_pass);
  }
  
  if (httpState != HTTP_IDLE) {
    return;
  }

  Serial.print("Connecting to server for location: ");
  Serial.println(data[fetchLocationIndex].queryName);
  
  if (client.connect(serverHost, serverPort)) {
    Serial.println("Connected! Sending POST request...");
    
    String postData = "time=now&location=" + String(data[fetchLocationIndex].queryName);
    
    client.println("POST " + String(serverPath) + " HTTP/1.1");
    client.println("Host: " + String(serverHost));
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(postData.length());
    client.println("Connection: close");
    client.println();
    client.print(postData);
    client.println();
    
    httpState = HTTP_AWAITING_RESPONSE;
    requestSendTime = millis();
  } else {
    Serial.println("Connection to server failed.");
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
    client.stop();
    httpState = HTTP_IDLE;
    
    body.trim();
    Serial.print("Server Response Body: ");
    Serial.println(body);
    
    int comma1 = body.indexOf(',');
    int comma2 = body.indexOf(',', comma1 + 1);
    if (comma1 != -1 && comma2 != -1) {
      int idx = fetchLocationIndex;
      data[idx].time = body.substring(0, comma1);
      data[idx].temp = body.substring(comma1 + 1, comma2);
      data[idx].humid = body.substring(comma2 + 1);
      
      data[idx].time.trim();
      data[idx].temp.trim();
      data[idx].humid.trim();
      data[idx].hasData = true;
      
      Serial.print("Successfully updated weather data for ");
      Serial.println(data[idx].queryName);
      
      // Move to the next location for the next request interval
      fetchLocationIndex = (fetchLocationIndex + 1) % 4;
    } else {
      Serial.println("Parsing error: response format invalid.");
    }
  } else if (millis() - requestSendTime > responseTimeout) {
    Serial.println("HTTP Client Timeout!");
    client.stop();
    httpState = HTTP_IDLE;
  }
}

void setup() {
  Serial.begin(115200);
  while(!Serial);
  matrix.begin();
  while(WiFi.status() == WL_NO_MODULE);
  
  wifi_status = WiFi.begin(wifi_ssid, wifi_pass);
  while (wifi_status != WL_CONNECTED) {
    delay(2000);
    Serial.print(".");
    wifi_status = WiFi.status();
  }

  Serial.println("\nConnected to WiFi!");
  setupSlide();
  lastDisplayStateTime = millis();
}

void loop() {
  loopCount++;

  unsigned long currentMillis = millis();
  handleDisplay();
  
  if (currentMillis - lastRequestTime >= requestInterval) {
    lastRequestTime = currentMillis;
    sendRequest();
  }
  handleResponse();
}