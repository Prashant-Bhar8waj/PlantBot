// XIAO ESP32-S3 Sun Tracking Robot with Telegram Control
// Moves along 30cm track to find and track brightest light position

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP32Servo.h>
#include "credentials.h"  // WiFi/telegram credentials (not pushed to GitHub)

// Hardware pin definitions
#define SERVO_PIN 1   // D0
#define LDR_PIN 2     // D1 (analog capable)

// Servo calibration
#define SERVO_STOP    96   // Neutral (stops at 94-98)
#define SERVO_FWD     0    // Forward (toward end)
#define SERVO_BWD     180  // Backward (toward start)

// Travel calibration
#define TOTAL_DISTANCE_CM  30.0
#define TOTAL_TIME_MS      7060   // 30cm in 7.06 seconds
#define SCAN_STEPS         10     // Number of measurement points

// Auto-tracking
#define AUTO_SCAN_INTERVAL 300000  // 5 minutes in milliseconds

Servo myServo;
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

float currentPosition = 0.0;
bool autoTrackingEnabled = false;
unsigned long lastAutoScan = 0;
int lastLightValue = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Sun Tracking Robot ===");
  
  // Setup servo
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(SERVO_STOP);
  
  // Setup LDR
  pinMode(LDR_PIN, INPUT);
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed!");
    Serial.print("Status: ");
    Serial.println(WiFi.status());
    Serial.println("Check SSID and password!");
  }
  
  // Setup Telegram
  client.setInsecure();
  
  // Send startup message
  String msg = "🌞 Sun Tracker Online!\n\n";
  msg += "Commands:\n";
  msg += "/scan - Scan track and show light values\n";
  msg += "/track - Find and move to brightest spot\n";
  msg += "/auto - Enable auto-tracking (every 5 min)\n";
  msg += "/stop - Disable auto-tracking\n";
  msg += "/home - Return to start (0cm)\n";
  msg += "/end - Move to end (30cm)\n";
  msg += "/status - Show current position & light\n";
  msg += "/reset - Reset position to 0cm";
  
  bot.sendMessage(chatID, msg, "");
  Serial.println("Telegram bot ready!");
}

void loop() {
  // Poll Telegram for new messages
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  
  while (numNewMessages) {
    handleMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
  
  // Auto-tracking mode
  if (autoTrackingEnabled) {
    if (millis() - lastAutoScan >= AUTO_SCAN_INTERVAL) {
      bot.sendMessage(chatID, "🔄 Auto-scan starting...", "");
      performTrack();
      lastAutoScan = millis();
    }
  }
  
  delay(1000);
}

void handleMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    
    Serial.println("Received: " + text);
    
    if (text == "/start" || text == "/help") {
      String msg = "🌞 Sun Tracker Commands:\n\n";
      msg += "/scan - Scan full track\n";
      msg += "/track - Move to brightest spot\n";
      msg += "/auto - Auto-track every 5 min\n";
      msg += "/stop - Stop auto-tracking\n";
      msg += "/home - Go to start\n";
      msg += "/end - Go to end\n";
      msg += "/status - Current info\n";
      msg += "/reset - Reset position";
      bot.sendMessage(chat_id, msg, "");
    }
    else if (text == "/scan") {
      performScan(chat_id);
    }
    else if (text == "/track") {
      performTrack();
    }
    else if (text == "/auto") {
      autoTrackingEnabled = true;
      lastAutoScan = millis();
      bot.sendMessage(chat_id, "✅ Auto-tracking ENABLED\nWill scan every 5 minutes", "");
    }
    else if (text == "/stop") {
      autoTrackingEnabled = false;
      myServo.write(SERVO_STOP);
      bot.sendMessage(chat_id, "⏹ Auto-tracking DISABLED", "");
    }
    else if (text == "/home") {
      bot.sendMessage(chat_id, "🏠 Going to START (0cm)...", "");
      goHome();
      bot.sendMessage(chat_id, "✅ At home position", "");
    }
    else if (text == "/end") {
      bot.sendMessage(chat_id, "🎯 Going to END (30cm)...", "");
      goToEnd();
      bot.sendMessage(chat_id, "✅ At end position", "");
    }
    else if (text == "/status") {
      int light = readLight();
      String msg = "📊 Status:\n\n";
      msg += "Position: " + String(currentPosition, 1) + " cm\n";
      msg += "Light level: " + String(light) + "\n";
      msg += "Auto-tracking: " + String(autoTrackingEnabled ? "ON" : "OFF");
      bot.sendMessage(chat_id, msg, "");
    }
    else if (text == "/reset") {
      currentPosition = 0.0;
      bot.sendMessage(chat_id, "🔄 Position reset to 0cm (home)", "");
    }
  }
}

void performScan(String chat_id) {
  bot.sendMessage(chat_id, "🔍 Starting scan...", "");
  
  goHome();
  delay(500);
  
  String results = "📊 Scan Results:\n\n";
  float stepSize = TOTAL_DISTANCE_CM / (SCAN_STEPS - 1);
  
  for (int i = 0; i < SCAN_STEPS; i++) {
    float targetPos = i * stepSize;
    
    // Move to position
    float distance = targetPos - currentPosition;
    if (abs(distance) > 0.1) {
      moveDistance(distance);
      delay(500);
    }
    
    // Measure light
    int lightValue = readLight();
    
    results += String(currentPosition, 1) + "cm → " + String(lightValue) + "\n";
    
    Serial.print("Position: ");
    Serial.print(currentPosition, 1);
    Serial.print(" cm | Light: ");
    Serial.println(lightValue);
  }
  
  bot.sendMessage(chat_id, results, "");
  goHome();
  bot.sendMessage(chat_id, "✅ Scan complete, returned home", "");
}

void performTrack() {
  Serial.println("=== Tracking brightest spot ===");
  
  goHome();
  delay(500);
  
  float stepSize = TOTAL_DISTANCE_CM / (SCAN_STEPS - 1);
  float brightestPosition = 0.0;
  int brightestValue = 0;
  
  // Scan and find brightest
  for (int i = 0; i < SCAN_STEPS; i++) {
    float targetPos = i * stepSize;
    
    float distance = targetPos - currentPosition;
    if (abs(distance) > 0.1) {
      moveDistance(distance);
      delay(500);
    }
    
    int lightValue = readLight();
    
    if (lightValue > brightestValue) {
      brightestValue = lightValue;
      brightestPosition = currentPosition;
    }
    
    Serial.print(currentPosition, 1);
    Serial.print("cm: ");
    Serial.println(lightValue);
  }
  
  // Move to brightest position
  Serial.print("Brightest at: ");
  Serial.print(brightestPosition, 1);
  Serial.print("cm (");
  Serial.print(brightestValue);
  Serial.println(")");
  
  moveToPosition(brightestPosition);
  
  String msg = "☀️ Moved to brightest spot!\n\n";
  msg += "Position: " + String(brightestPosition, 1) + " cm\n";
  msg += "Light level: " + String(brightestValue);
  bot.sendMessage(chatID, msg, "");
  
  lastLightValue = brightestValue;
}

void goHome() {
  Serial.println("Going to START (0cm)");
  moveToPosition(0.0);
}

void goToEnd() {
  Serial.println("Going to END (30cm)");
  moveToPosition(TOTAL_DISTANCE_CM);
}

void moveToPosition(float targetCm) {
  float distance = targetCm - currentPosition;
  moveDistance(distance);
}

void moveDistance(float distanceCm) {
  if (abs(distanceCm) < 0.1) return;
  
  float timeMs = (abs(distanceCm) / TOTAL_DISTANCE_CM) * TOTAL_TIME_MS;
  
  Serial.print("Moving ");
  Serial.print(abs(distanceCm), 1);
  Serial.print("cm ");
  Serial.print(distanceCm > 0 ? "FORWARD" : "BACKWARD");
  Serial.print(" (");
  Serial.print((int)timeMs);
  Serial.println("ms)");
  
  if (distanceCm > 0) {
    myServo.write(SERVO_FWD);
  } else {
    myServo.write(SERVO_BWD);
  }
  
  delay((int)timeMs);
  myServo.write(SERVO_STOP);
  
  currentPosition += distanceCm;
  currentPosition = constrain(currentPosition, 0, TOTAL_DISTANCE_CM);
}

int readLight() {
  // Read LDR value (0-4095 on ESP32)
  return analogRead(LDR_PIN);
}
