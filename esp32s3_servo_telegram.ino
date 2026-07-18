// ESP32-S3 Servo Control via Telegram Bot
// Control servo position through Telegram commands

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP32Servo.h>

#include "credentials.h"

Servo myServo;

#define SERVO_PIN 18

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

unsigned long lastBotCheck = 0;
int currentPos = 90;

void setup() {
  Serial.begin(115200);
  Serial.println("\nESP32-S3 Servo Telegram Control");
  
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(currentPos);
  Serial.println("Servo initialized at 90°");
  
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
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed");
  }
  
  client.setInsecure();
  
  bot.sendMessage(chatID, "ESP32-S3 Servo Control Online!\n\nCommands:\n/servo <0-180> - Set position\n/center - Move to 90°\n/status - Get current position", "");
  Serial.println("Bot ready");
}

void loop() {
  if (millis() - lastBotCheck > 1000) {
    int numNew = bot.getUpdates(bot.last_message_received + 1);
    
    if (numNew) {
      handleMessages(numNew);
    }
    
    lastBotCheck = millis();
  }
}

void handleMessages(int numMsgs) {
  for (int i = 0; i < numMsgs; i++) {
    String chat = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    
    Serial.println("Received: " + text);
    
    if (text == "/start") {
      String msg = "ESP32-S3 Servo Control\n\n";
      msg += "Commands:\n";
      msg += "/servo <angle> - Set position (0-180)\n";
      msg += "/center - Move to center (90°)\n";
      msg += "/status - Current position\n";
      msg += "/min - Move to 0°\n";
      msg += "/max - Move to 180°\n";
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/status") {
      String msg = "Servo Status:\n\n";
      msg += "Current Position: " + String(currentPos) + "°\n";
      msg += "Pin: GPIO " + String(SERVO_PIN);
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/center") {
      moveServo(90);
      bot.sendMessage(chat, "Servo moved to center (90°)", "");
    }
    else if (text == "/min") {
      moveServo(0);
      bot.sendMessage(chat, "Servo moved to minimum (0°)", "");
    }
    else if (text == "/max") {
      moveServo(180);
      bot.sendMessage(chat, "Servo moved to maximum (180°)", "");
    }
    else if (text.startsWith("/servo ")) {
      int pos = text.substring(7).toInt();
      
      if (pos >= 0 && pos <= 180) {
        moveServo(pos);
        String msg = "Servo moved to " + String(pos) + "°";
        bot.sendMessage(chat, msg, "");
      } else {
        bot.sendMessage(chat, "Invalid angle!\n\nUse: /servo <0-180>\nExample: /servo 90", "");
      }
    }
    else {
      bot.sendMessage(chat, "Unknown command\nTry /start for help", "");
    }
  }
}

void moveServo(int pos) {
  pos = constrain(pos, 0, 180);
  myServo.write(pos);
  currentPos = pos;
  
  Serial.print("Servo moved to: ");
  Serial.print(pos);
  Serial.println("°");
}
