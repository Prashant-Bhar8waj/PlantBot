// Complete Plant Monitoring System with Camera
// ESP8266 for sensors + XIAO ESP32S3 for camera
// This version runs on ESP8266 (sensors + telegram commands)
// Upload esp32cam_plant_identify.ino separately to XIAO ESP32S3

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <DHT.h>

#include "credentials.h"

// OLED display
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);

// pins
#define MOISTURE_PIN A0
#define RED_LED 13      // D7
#define YELLOW_LED 15   // D8
#define GREEN_LED 14    // D5
#define TRIG_PIN 12     // D6
#define ECHO_PIN 16     // D0
#define DHT_PIN 2       // D4

// moisture calibration values (found through testing)
#define DRY_VAL 95
#define WET_VAL 64

// temp sensor setup - had to calibrate because readings were off
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);
#define TEMP_OFFSET 20.0
#define HUMIDITY_OFFSET 35.0

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

// timing variables
unsigned long lastBotCheck;
unsigned long lastAlert = 0;
unsigned long lastWatered = 0;
unsigned long displayTimer = 0;

// sensor values
int moisture = 0;
float temp = 0;
float humidity = 0;
int distance = 0;
bool displayOn = true;

// settings
String plantName = "My Plant";
int alertThreshold = 30;
int moistureHistory[10];
int historyIndex = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);  // SDA=D2, SCL=D1
  
  // init OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed");
    while (1);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Starting...");
  display.display();
  
  // init pins
  pinMode(MOISTURE_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(DHT_PIN, INPUT_PULLUP);
  
  // all LEDs off
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  
  dht.begin();
  
  // connect wifi
  Serial.print("connecting to wifi");
  display.println("WiFi...");
  display.display();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nwifi OK");
  display.println("WiFi OK");
  display.display();
  
  client.setInsecure();
  
  // startup message
  String msg = "🌱 Plant Monitor Online!\n\n";
  msg += "All sensors ready\n";
  msg += "Camera module ready\n\n";
  msg += "Send /help for commands";
  bot.sendMessage(chatID, msg, "");
  
  delay(2000);
}

void readSensors() {
  // moisture
  int raw = analogRead(MOISTURE_PIN);
  moisture = map(raw, WET_VAL, DRY_VAL, 100, 0);
  moisture = constrain(moisture, 0, 100);
  
  // store in history
  moistureHistory[historyIndex] = moisture;
  historyIndex = (historyIndex + 1) % 10;
  
  // DHT with retry
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  
  if (!isnan(t) && !isnan(h)) {
    temp = t + TEMP_OFFSET;
    humidity = h + HUMIDITY_OFFSET;
  }
  
  // ultrasonic
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) {
    distance = 999;
  } else {
    distance = duration * 0.034 / 2;
  }
}

void updateLEDs() {
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  
  if (moisture < 20) {
    digitalWrite(RED_LED, HIGH);
  } else if (moisture < 40) {
    digitalWrite(YELLOW_LED, HIGH);
  } else {
    digitalWrite(GREEN_LED, HIGH);
  }
}

void updateDisplay() {
  if (!displayOn) return;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  display.println(plantName);
  display.println("---");
  
  display.print("Moisture: ");
  display.print(moisture);
  display.println("%");
  
  display.print("Temp: ");
  display.print(temp, 1);
  display.println("C");
  
  display.print("Humidity: ");
  display.print(humidity, 0);
  display.println("%");
  
  // status
  if (moisture < 20) {
    display.println("Status: DRY!");
  } else if (moisture < 40) {
    display.println("Status: Low");
  } else {
    display.println("Status: Good");
  }
  
  display.display();
}

void checkProximity() {
  if (distance < 50) {
    displayOn = true;
    displayTimer = millis();
  } else if (millis() - displayTimer > 30000) {
    displayOn = false;
    display.clearDisplay();
    display.display();
  }
}

void checkAlert() {
  if (moisture < alertThreshold && millis() - lastAlert > 300000) {
    String msg = "⚠️ ALERT!\n\n";
    msg += plantName + " needs water!\n";
    msg += "Moisture: " + String(moisture) + "%\n";
    msg += "Threshold: " + String(alertThreshold) + "%";
    bot.sendMessage(chatID, msg, "");
    lastAlert = millis();
  }
}

void handleMessages(int numMsgs) {
  for (int i = 0; i < numMsgs; i++) {
    String chat = bot.messages[i].chat_id;
    String text = bot.messages[i].text;
    
    Serial.println("msg: " + text);
    
    if (text == "/start") {
      String msg = "🌱 Welcome to Plant Monitor!\n\n";
      msg += "Commands:\n";
      msg += "/status - current readings\n";
      msg += "/identify - identify plant (camera)\n";
      msg += "/water - mark as watered\n";
      msg += "/history - moisture history\n";
      msg += "/environment - temp/humidity\n";
      msg += "/setname <name> - set plant name\n";
      msg += "/setalert <value> - set alert %\n";
      msg += "/displayon - keep display on\n";
      msg += "/help - show commands";
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/status") {
      String msg = "🌱 " + plantName + "\n\n";
      msg += "💧 Moisture: " + String(moisture) + "%\n";
      msg += "🌡️ Temperature: " + String(temp, 1) + "°C\n";
      msg += "💨 Humidity: " + String(humidity, 0) + "%\n\n";
      
      if (moisture < 20) {
        msg += "Status: VERY DRY 🔴\n⚠️ Water now!";
      } else if (moisture < 40) {
        msg += "Status: DRY 🟡\nShould water soon";
      } else {
        msg += "Status: GOOD ✅\nPlant is happy!";
      }
      
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/identify") {
      // camera module handles this command
      // just acknowledge here
      bot.sendMessage(chat, "📸 Camera module processing...\nCheck for result in a moment", "");
    }
    else if (text == "/water") {
      lastWatered = millis();
      bot.sendMessage(chat, "✅ Marked as watered!\nI'll remind you when it needs more", "");
    }
    else if (text == "/history") {
      String msg = "📊 Moisture History (last 10):\n\n";
      for (int j = 0; j < 10; j++) {
        int idx = (historyIndex + j) % 10;
        msg += String(j + 1) + ". " + String(moistureHistory[idx]) + "%\n";
      }
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/environment") {
      String msg = "🌡️ Environment Details:\n\n";
      msg += "Temperature: " + String(temp, 1) + "°C\n";
      msg += "Humidity: " + String(humidity, 1) + "%\n\n";
      
      if (temp < 15) {
        msg += "⚠️ Too cold for most plants";
      } else if (temp > 30) {
        msg += "⚠️ Too hot, provide shade";
      } else {
        msg += "✅ Temperature is good";
      }
      
      bot.sendMessage(chat, msg, "");
    }
    else if (text.startsWith("/setname ")) {
      plantName = text.substring(9);
      bot.sendMessage(chat, "✅ Plant name set to: " + plantName, "");
    }
    else if (text.startsWith("/setalert ")) {
      alertThreshold = text.substring(10).toInt();
      String msg = "✅ Alert threshold set to " + String(alertThreshold) + "%";
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/displayon") {
      displayOn = true;
      displayTimer = millis() + 3600000; // 1 hour
      bot.sendMessage(chat, "✅ Display will stay on for 1 hour", "");
    }
    else if (text == "/help") {
      String msg = "🌿 Plant Monitor Commands:\n\n";
      msg += "/status - current sensor readings\n";
      msg += "/identify - identify plant with camera\n";
      msg += "/water - mark plant as watered\n";
      msg += "/history - last 10 moisture readings\n";
      msg += "/environment - temperature & humidity\n";
      msg += "/setname <name> - change plant name\n";
      msg += "/setalert <value> - set moisture alert %\n";
      msg += "/displayon - keep display on for 1 hour\n\n";
      msg += "🔔 Auto alerts when moisture < " + String(alertThreshold) + "%";
      bot.sendMessage(chat, msg, "");
    }
    else {
      bot.sendMessage(chat, "Unknown command. Try /help", "");
    }
  }
}

void loop() {
  // read sensors every 500ms
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 500) {
    readSensors();
    updateLEDs();
    updateDisplay();
    checkProximity();
    checkAlert();
    lastRead = millis();
  }
  
  // check telegram every 1 second
  if (millis() - lastBotCheck > 1000) {
    int msgs = bot.getUpdates(bot.last_message_received + 1);
    if (msgs > 0) {
      handleMessages(msgs);
    }
    lastBotCheck = millis();
  }
}
