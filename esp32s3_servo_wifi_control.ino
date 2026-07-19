// ESP32-S3 Servo Control via WiFi Web Server
// Control servo position through web interface

#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

#include "credentials.h"

Servo myServo;

#define SERVO_PIN 18

WebServer server(80);

int currentPos = 90;

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32-S3 Servo Web Control");
  
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(currentPos);
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Open this IP in your browser to control servo");
  } else {
    Serial.println("\nWiFi connection failed");
  }
  
  server.on("/", handleRoot);
  server.on("/servo", handleServo);
  
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial; text-align: center; margin: 20px; background: #f0f0f0; }";
  html += "h1 { color: #333; }";
  html += ".container { background: white; padding: 30px; border-radius: 10px; max-width: 500px; margin: 0 auto; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
  html += ".slider { width: 100%; height: 50px; margin: 20px 0; }";
  html += ".value { font-size: 48px; color: #4CAF50; font-weight: bold; margin: 20px 0; }";
  html += ".buttons { margin: 20px 0; }";
  html += "button { background: #4CAF50; color: white; border: none; padding: 15px 30px; font-size: 18px; margin: 5px; border-radius: 5px; cursor: pointer; }";
  html += "button:hover { background: #45a049; }";
  html += "button:active { background: #3d8b40; }";
  html += "</style></head><body>";
  html += "<div class='container'>";
  html += "<h1>ESP32-S3 Servo Control</h1>";
  html += "<div class='value' id='posValue'>" + String(currentPos) + "°</div>";
  html += "<input type='range' min='0' max='180' value='" + String(currentPos) + "' class='slider' id='servoSlider' oninput='updateServo(this.value)'>";
  html += "<div class='buttons'>";
  html += "<button onclick='setPosition(0)'>0°</button>";
  html += "<button onclick='setPosition(45)'>45°</button>";
  html += "<button onclick='setPosition(90)'>90°</button>";
  html += "<button onclick='setPosition(135)'>135°</button>";
  html += "<button onclick='setPosition(180)'>180°</button>";
  html += "</div>";
  html += "<p>Current Position: <span id='current'>" + String(currentPos) + "°</span></p>";
  html += "</div>";
  html += "<script>";
  html += "function updateServo(pos) {";
  html += " document.getElementById('posValue').innerHTML = pos + '°';";
  html += " fetch('/servo?pos=' + pos);";
  html += " document.getElementById('current').innerHTML = pos + '°';";
  html += "}";
  html += "function setPosition(pos) {";
  html += " document.getElementById('servoSlider').value = pos;";
  html += " updateServo(pos);";
  html += "}";
  html += "</script></body></html>";
  
  server.send(200, "text/html", html);
}

void handleServo() {
  if (server.hasArg("pos")) {
    int pos = server.arg("pos").toInt();
    pos = constrain(pos, 0, 180);
    
    myServo.write(pos);
    currentPos = pos;
    
    Serial.print("Servo moved to: ");
    Serial.print(pos);
    Serial.println("°");
    
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing position parameter");
  }
}
