#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Moisture sensor
#define MOISTURE_PIN A0
#define DRY_VALUE 95    // Sensor in air (your calibration: 93-96)
#define WET_VALUE 64    // Sensor in water (your calibration: 63-65)

// LED pin definitions
#define RED_LED 13      // D7 - Very dry
#define YELLOW_LED 15   // D8 - Dry
#define GREEN_LED 14    // D5 - Good

// Credentials are loaded from credentials.h (gitignored)
#include "credentials.h"

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

unsigned long lastTimeBotRan;
unsigned long lastMoistureAlert = 0;
unsigned long lastWateredTime = 0;
const unsigned long botRequestDelay = 1000;
const unsigned long alertInterval = 300000; // 5 minutes between alerts

int currentMoisture = 0;
int customAlertThreshold = 30; // Default alert threshold
String plantName = "My Plant"; // Default plant name

// Moisture history (stores last 10 readings)
int moistureHistory[10];
int historyIndex = 0;
bool historyFull = false;

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);
  
  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed!");
    while(1);
  }
  
  // Show startup message
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Plant Robot");
  display.println("Starting...");
  display.display();
  
  pinMode(MOISTURE_PIN, INPUT);
  
  // Initialize LED pins
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  
  // Turn off all LEDs initially
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  display.setCursor(0, 20);
  display.println("Connecting WiFi...");
  display.display();
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  display.println("WiFi OK!");
  display.display();
  
  client.setInsecure();
  
  // Send startup message to Telegram
  bot.sendMessage(chatID, "🌱 Plant Robot is online!\n\nSend /help for commands", "");
  Serial.println("Startup message sent!");
  
  delay(2000);
}

void readMoisture() {
  int moistureRaw = analogRead(MOISTURE_PIN);
  currentMoisture = map(moistureRaw, WET_VALUE, DRY_VALUE, 100, 0);
  currentMoisture = constrain(currentMoisture, 0, 100);
  
  // Store in history
  moistureHistory[historyIndex] = currentMoisture;
  historyIndex++;
  if (historyIndex >= 10) {
    historyIndex = 0;
    historyFull = true;
  }
  
  Serial.print("Raw: ");
  Serial.print(moistureRaw);
  Serial.print(" | Moisture: ");
  Serial.print(currentMoisture);
  Serial.println("%");
}

void updateLEDs() {
  // Turn off all LEDs first
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  
  // Turn on appropriate LED based on moisture level
  if (currentMoisture < 20) {
    digitalWrite(RED_LED, HIGH);      // Very dry - RED
  } else if (currentMoisture < 40) {
    digitalWrite(YELLOW_LED, HIGH);   // Dry - YELLOW
  } else {
    digitalWrite(GREEN_LED, HIGH);    // Good - GREEN
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("PLANT MONITOR");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  // Moisture percentage - BIG
  display.setTextSize(3);
  display.setCursor(10, 20);
  display.print(currentMoisture);
  display.print("%");
  
  // Status text
  display.setTextSize(1);
  display.setCursor(0, 50);
  
  if (currentMoisture < 20) {
    display.print("VERY DRY!");
  } else if (currentMoisture < 40) {
    display.print("DRY - Water me!");
  } else if (currentMoisture < 70) {
    display.print("GOOD");
  } else {
    display.print("WET");
  }
  
  // WiFi indicator
  display.setCursor(100, 0);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("WiFi");
  }
  
  display.display();
}

void checkMoistureAlert() {
  // Send alert if moisture is low and enough time has passed
  if (currentMoisture < customAlertThreshold && (millis() - lastMoistureAlert > alertInterval)) {
    String message = "⚠️ MOISTURE ALERT!\n\n";
    message += plantName + " needs attention!\n";
    message += "Current level: " + String(currentMoisture) + "%\n";
    message += "Alert threshold: " + String(customAlertThreshold) + "%\n\n";
    message += "💧 Time to water your plant!";
    
    bot.sendMessage(chatID, message, "");
    lastMoistureAlert = millis();
    Serial.println("Low moisture alert sent!");
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    
    Serial.println("Received: " + text);
    
    if (text == "/start") {
      String welcome = "🌱 Welcome to Plant Robot!\n\n";
      welcome += "I monitor " + plantName + "'s soil moisture and send alerts.\n\n";
      welcome += "Commands:\n";
      welcome += "/status - Check current moisture\n";
      welcome += "/help - Show all commands";
      bot.sendMessage(chat_id, welcome, "");
    }
    else if (text == "/status") {
      String status = "🌱 " + plantName + " Status:\n\n";
      status += "💧 Moisture: " + String(currentMoisture) + "%\n";
      status += "🔔 Alert at: " + String(customAlertThreshold) + "%\n\n";
      
      if (currentMoisture < 20) {
        status += "Status: VERY DRY! 🔴\n";
        status += "⚠️ Water immediately!";
      } else if (currentMoisture < 40) {
        status += "Status: DRY 🟡\n";
        status += "💡 Should water soon";
      } else if (currentMoisture < 70) {
        status += "Status: GOOD ✅\n";
        status += "😊 Plant is happy!";
      } else {
        status += "Status: WET 💦\n";
        status += "✋ Don't water yet";
      }
      
      // Add time since last watered
      if (lastWateredTime > 0) {
        unsigned long hoursSince = (millis() - lastWateredTime) / 3600000;
        status += "\n\n⏱️ Last watered: " + String(hoursSince) + "h ago";
      }
      
      bot.sendMessage(chat_id, status, "");
    }
    else if (text == "/help") {
      String help = "🌿 Plant Robot Commands:\n\n";
      help += "📊 Monitoring:\n";
      help += "/status - Current status\n";
      help += "/history - Last 10 readings\n\n";
      help += "💧 Watering:\n";
      help += "/water - Mark as watered\n";
      help += "/lastwatered - Time since watering\n\n";
      help += "⚙️ Settings:\n";
      help += "/setname <name> - Set plant name\n";
      help += "/setalert <value> - Set alert level\n\n";
      help += "🔔 Auto alerts at " + String(customAlertThreshold) + "%";
      bot.sendMessage(chat_id, help, "");
    }
    else if (text == "/water") {
      lastWateredTime = millis();
      lastMoistureAlert = millis(); // Reset alert timer
      String msg = "💧 " + plantName + " has been watered!\n\n";
      msg += "Current moisture: " + String(currentMoisture) + "%\n";
      msg += "I'll remind you if it gets dry again.";
      bot.sendMessage(chat_id, msg, "");
      Serial.println("Plant marked as watered");
    }
    else if (text == "/lastwatered") {
      if (lastWateredTime == 0) {
        bot.sendMessage(chat_id, "⏱️ No watering recorded yet.\n\nUse /water after watering your plant!", "");
      } else {
        unsigned long hoursSince = (millis() - lastWateredTime) / 3600000;
        unsigned long minutesSince = ((millis() - lastWateredTime) % 3600000) / 60000;
        String msg = "⏱️ Last Watered:\n\n";
        msg += String(hoursSince) + "h " + String(minutesSince) + "m ago\n\n";
        msg += "Current moisture: " + String(currentMoisture) + "%";
        bot.sendMessage(chat_id, msg, "");
      }
    }
    else if (text == "/history") {
      String hist = "📊 Moisture History:\n\n";
      int count = historyFull ? 10 : historyIndex;
      if (count == 0) {
        hist += "No data yet. Wait a few readings.";
      } else {
        int startIdx = historyFull ? historyIndex : 0;
        for (int j = 0; j < count; j++) {
          int idx = (startIdx + j) % 10;
          hist += String(count - j) + ". " + String(moistureHistory[idx]) + "%\n";
        }
        // Calculate average
        int sum = 0;
        for (int j = 0; j < count; j++) {
          sum += moistureHistory[j];
        }
        int avg = sum / count;
        hist += "\n📈 Average: " + String(avg) + "%";
      }
      bot.sendMessage(chat_id, hist, "");
    }
    else if (text.startsWith("/setname ")) {
      plantName = text.substring(9);
      plantName.trim();
      if (plantName.length() > 0) {
        String msg = "✅ Plant name set to: " + plantName;
        bot.sendMessage(chat_id, msg, "");
        Serial.println("Plant name: " + plantName);
      } else {
        bot.sendMessage(chat_id, "❌ Please provide a name.\n\nExample: /setname Basil", "");
      }
    }
    else if (text.startsWith("/setalert ")) {
      int newThreshold = text.substring(10).toInt();
      if (newThreshold > 0 && newThreshold <= 100) {
        customAlertThreshold = newThreshold;
        String msg = "✅ Alert threshold set to " + String(customAlertThreshold) + "%\n\n";
        msg += "I'll notify you when moisture drops below this level.";
        bot.sendMessage(chat_id, msg, "");
        Serial.println("Alert threshold: " + String(customAlertThreshold));
      } else {
        bot.sendMessage(chat_id, "❌ Invalid value. Use 1-100.\n\nExample: /setalert 25", "");
      }
    }
    else {
      bot.sendMessage(chat_id, "Unknown command. Try /help", "");
    }
  }
}

void loop() {
  // Read moisture sensor
  readMoisture();
  
  // Update LEDs based on moisture
  updateLEDs();
  
  // Update OLED display
  updateDisplay();
  
  // Check if moisture alert needed
  checkMoistureAlert();
  
  // Poll Telegram for new messages
  if (millis() - lastTimeBotRan > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    
    if (numNewMessages) {
      handleNewMessages(numNewMessages);
    }
    
    lastTimeBotRan = millis();
  }
  
  delay(500);
}
