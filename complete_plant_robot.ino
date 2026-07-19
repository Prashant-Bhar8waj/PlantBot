#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <DHT.h>

// OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Moisture sensor
#define MOISTURE_PIN A0
#define DRY_VALUE 95
#define WET_VALUE 64

// LED pin definitions
#define RED_LED 13      // D7
#define YELLOW_LED 15   // D8
#define GREEN_LED 14    // D5

// Ultrasonic Sensor
#define TRIG_PIN 12     // D6
#define ECHO_PIN 16     // D0

// DHT Sensor
#define DHT_PIN 2       // D4
#define DHT_TYPE DHT11  // Using DHT11 (3-pin sensor)
DHT dht(DHT_PIN, DHT_TYPE);

// DHT Calibration - Adjust these based on your room conditions
#define TEMP_OFFSET 20.0      // Add to raw temp reading
#define HUMIDITY_OFFSET 35.0  // Add to raw humidity reading

// Credentials are loaded from credentials.h (gitignored)
#include "credentials.h"WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

unsigned long lastTimeBotRan;
unsigned long lastMoistureAlert = 0;
unsigned long lastWateredTime = 0;
unsigned long displayOffTime = 0;
const unsigned long botRequestDelay = 1000;
const unsigned long alertInterval = 300000;
const unsigned long displayTimeout = 30000; // Display turns off after 30 seconds

int currentMoisture = 0;
float currentTemp = 0;
float currentHumidity = 0;
int customAlertThreshold = 30;
String plantName = "My Plant";
bool displayOn = false;
int detectionDistance = 50; // Turn on display if closer than 50cm

// Moisture history
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
  display.println("Initializing...");
  display.display();
  
  pinMode(MOISTURE_PIN, INPUT);
  
  // Initialize LED pins
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  
  // Initialize Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Initialize DHT with pull-up
  pinMode(DHT_PIN, INPUT_PULLUP);
  dht.begin();
  
  // Wait for DHT to stabilize
  delay(2000);
  
  // Test DHT reading
  float testTemp = dht.readTemperature();
  if (isnan(testTemp)) {
    Serial.println("DHT sensor not responding!");
    display.setCursor(0, 30);
    display.println("DHT Error!");
    display.display();
  } else {
    Serial.print("DHT OK! Temp: ");
    Serial.println(testTemp);
  }
  
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
  
  // Send startup message
  bot.sendMessage(chatID, "Plant Robot is online!\n\nSend /help for commands", "");
  Serial.println("Startup message sent!");
  
  delay(2000);
  displayOn = true;
  displayOffTime = millis();
}

int getDistance() {
  // Send ultrasonic pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read echo
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout
  
  // Calculate distance in cm
  int distance = duration * 0.034 / 2;
  
  // Return 999 if no reading
  if (distance == 0) return 999;
  
  return distance;
}

void readSensors() {
  // Read moisture
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
  
  // Read temperature and humidity with retry
  float rawTemp = dht.readTemperature();
  float rawHumidity = dht.readHumidity();
  
  // Retry once if failed
  if (isnan(rawTemp) || isnan(rawHumidity)) {
    delay(100);
    rawTemp = dht.readTemperature();
    rawHumidity = dht.readHumidity();
  }
  
  // Apply calibration and check validity
  if (!isnan(rawTemp)) {
    currentTemp = rawTemp + TEMP_OFFSET;
    Serial.print("DHT Raw: ");
    Serial.print(rawTemp, 1);
    Serial.print("°C Calibrated: ");
    Serial.print(currentTemp, 1);
    Serial.print("°C | ");
  } else {
    currentTemp = 0;
    Serial.print("Temp: FAILED | ");
  }
  
  if (!isnan(rawHumidity)) {
    currentHumidity = rawHumidity + HUMIDITY_OFFSET;
    currentHumidity = constrain(currentHumidity, 0, 100);
    Serial.print("Raw: ");
    Serial.print(rawHumidity, 1);
    Serial.print("% Calibrated: ");
    Serial.print(currentHumidity, 1);
    Serial.println("%");
  } else {
    currentHumidity = 0;
    Serial.println("Humidity: FAILED");
  }
  
  Serial.print("Moisture: ");
  Serial.print(currentMoisture);
  Serial.print("% | Temp: ");
  Serial.print(currentTemp);
  Serial.print("°C | Humidity: ");
  Serial.print(currentHumidity);
  Serial.println("%");
}

void updateLEDs() {
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  
  if (currentMoisture < 20) {
    digitalWrite(RED_LED, HIGH);
  } else if (currentMoisture < 40) {
    digitalWrite(YELLOW_LED, HIGH);
  } else {
    digitalWrite(GREEN_LED, HIGH);
  }
}

void updateDisplay() {
  if (!displayOn) {
    display.clearDisplay();
    display.display();
    return;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Title
  display.setCursor(0, 0);
  display.println(plantName);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  // Moisture - Large
  display.setTextSize(2);
  display.setCursor(0, 15);
  display.print("M:");
  display.print(currentMoisture);
  display.println("%");
  
  // Temperature and Humidity
  display.setTextSize(1);
  display.setCursor(0, 35);
  if (currentTemp > 0) {
    display.print("Temp: ");
    display.print(currentTemp, 1);
    display.println("C");
  }
  
  display.setCursor(0, 45);
  if (currentHumidity > 0) {
    display.print("Humid: ");
    display.print(currentHumidity, 1);
    display.println("%");
  }
  
  // Status
  display.setCursor(0, 55);
  if (currentMoisture < 20) {
    display.print("VERY DRY!");
  } else if (currentMoisture < 40) {
    display.print("DRY");
  } else if (currentMoisture < 70) {
    display.print("GOOD");
  } else {
    display.print("WET");
  }
  
  // WiFi indicator
  display.setCursor(110, 0);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("W");
  }
  
  display.display();
}

void checkProximity() {
  int distance = getDistance();
  
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println("cm");
  
  // Turn on display if someone is close
  if (distance < detectionDistance && distance > 0) {
    if (!displayOn) {
      Serial.println("Person detected! Display ON");
    }
    displayOn = true;
    displayOffTime = millis();
  }
  
  // Turn off display after timeout
  if (displayOn && (millis() - displayOffTime > displayTimeout)) {
    Serial.println("Timeout - Display OFF");
    displayOn = false;
  }
}

void checkMoistureAlert() {
  if (currentMoisture < customAlertThreshold && (millis() - lastMoistureAlert > alertInterval)) {
    String message = "MOISTURE ALERT!\n\n";
    message += plantName + "needs attention!\n";
    message += "Moisture: " + String(currentMoisture) + "%\n";
    if (currentTemp > 0) {
      message += "Temp: " + String(currentTemp, 1) + "°C\n";
    }
    if (currentHumidity > 0) {
      message += "Humidity: " + String(currentHumidity, 1) + "%\n";
    }
    message += "\nTime to water your plant!";
    
    bot.sendMessage(chatID, message, "");
    lastMoistureAlert = millis();
    Serial.println("Alert sent!");
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    
    Serial.println("Received: " + text);
    
    if (text == "/start") {
      String welcome = "Welcome to Plant Robot!\n\n";
      welcome += "Monitoring " + plantName + "\n\n";
      welcome += "Commands:\n/status /help";
      bot.sendMessage(chat_id, welcome, "");
    }
    else if (text == "/status") {
      String status = " " + plantName + "Status:\n\n";
      status += "Moisture: " + String(currentMoisture) + "%\n";
      if (currentTemp > 0) {
        status += "Temperature: " + String(currentTemp, 1) + "°C\n";
      }
      if (currentHumidity > 0) {
        status += "Humidity: " + String(currentHumidity, 1) + "%\n";
      }
      status += "Alert at: " + String(customAlertThreshold) + "%\n\n";
      
      if (currentMoisture < 20) {
        status += "Status: VERY DRY! \n Water immediately!";
      } else if (currentMoisture < 40) {
        status += "Status: DRY \n Should water soon";
      } else if (currentMoisture < 70) {
        status += "Status: GOOD \n Plant is happy!";
      } else {
        status += "Status: WET \n Don't water yet";
      }
      
      if (lastWateredTime > 0) {
        unsigned long hoursSince = (millis() - lastWateredTime) / 3600000;
        status += "\n\n⏱ Last watered: " + String(hoursSince) + "h ago";
      }
      
      bot.sendMessage(chat_id, status, "");
    }
    else if (text == "/help") {
      String help = "Plant Robot Commands:\n\n";
      help += "Monitoring:\n";
      help += "/status - Full status\n";
      help += "/history - Last 10 readings\n";
      help += "/environment - Temp & humidity\n\n";
      help += "Watering:\n";
      help += "/water - Mark as watered\n";
      help += "/lastwatered - Time since watering\n\n";
      help += "Settings:\n";
      help += "/setname <name>\n";
      help += "/setalert <value>\n";
      help += "/displayon - Keep display on\n\n";
      help += "Auto alerts at " + String(customAlertThreshold) + "%";
      bot.sendMessage(chat_id, help, "");
    }
    else if (text == "/environment") {
      String env = "Environment Data:\n\n";
      if (currentTemp > 0) {
        env += "Temperature: " + String(currentTemp, 1) + "°C\n";
        if (currentTemp < 15) env += "Too cold!\n";
        else if (currentTemp > 30) env += "Too hot!\n";
        else env += "Good\n";
      }
      if (currentHumidity > 0) {
        env += "\n Humidity: " + String(currentHumidity, 1) + "%\n";
        if (currentHumidity < 30) env += "Too dry!\n";
        else if (currentHumidity > 70) env += "Too humid!\n";
        else env += "Good\n";
      }
      env += "\n Moisture: " + String(currentMoisture) + "%";
      bot.sendMessage(chat_id, env, "");
    }
    else if (text == "/water") {
      lastWateredTime = millis();
      lastMoistureAlert = millis();
      String msg = " " + plantName + "has been watered!\n\n";
      msg += "Current moisture: " + String(currentMoisture) + "%\n";
      msg += "I'll remind you if it gets dry again.";
      bot.sendMessage(chat_id, msg, "");
    }
    else if (text == "/lastwatered") {
      if (lastWateredTime == 0) {
        bot.sendMessage(chat_id, "⏱ No watering recorded yet.\n\nUse /water after watering!", "");
      } else {
        unsigned long hoursSince = (millis() - lastWateredTime) / 3600000;
        unsigned long minutesSince = ((millis() - lastWateredTime) % 3600000) / 60000;
        String msg = "⏱ Last Watered:\n\n";
        msg += String(hoursSince) + "h " + String(minutesSince) + "m ago\n\n";
        msg += "Current moisture: " + String(currentMoisture) + "%";
        bot.sendMessage(chat_id, msg, "");
      }
    }
    else if (text == "/history") {
      String hist = "Moisture History:\n\n";
      int count = historyFull ? 10 : historyIndex;
      if (count == 0) {
        hist += "No data yet.";
      } else {
        int startIdx = historyFull ? historyIndex : 0;
        for (int j = 0; j < count; j++) {
          int idx = (startIdx + j) % 10;
          hist += String(count - j) + ". " + String(moistureHistory[idx]) + "%\n";
        }
        int sum = 0;
        for (int j = 0; j < count; j++) {
          sum += moistureHistory[j];
        }
        int avg = sum / count;
        hist += "\n Average: " + String(avg) + "%";
      }
      bot.sendMessage(chat_id, hist, "");
    }
    else if (text == "/displayon") {
      displayOn = true;
      displayOffTime = millis() + 3600000; // Keep on for 1 hour
      bot.sendMessage(chat_id, "Display will stay on for 1 hour", "");
    }
    else if (text.startsWith("/setname ")) {
      plantName = text.substring(9);
      plantName.trim();
      if (plantName.length() > 0) {
        bot.sendMessage(chat_id, "Plant name set to: " + plantName, "");
      } else {
        bot.sendMessage(chat_id, "Please provide a name.\n\nExample: /setname Basil", "");
      }
    }
    else if (text.startsWith("/setalert ")) {
      int newThreshold = text.substring(10).toInt();
      if (newThreshold > 0 && newThreshold <= 100) {
        customAlertThreshold = newThreshold;
        String msg = "Alert threshold set to " + String(customAlertThreshold) + "%";
        bot.sendMessage(chat_id, msg, "");
      } else {
        bot.sendMessage(chat_id, "Invalid value. Use 1-100.\n\nExample: /setalert 25", "");
      }
    }
    else {
      bot.sendMessage(chat_id, "Unknown command. Try /help", "");
    }
  }
}

void loop() {
  // Read all sensors
  readSensors();
  
  // Update LEDs
  updateLEDs();
  
  // Check proximity and manage display
  checkProximity();
  
  // Update display
  updateDisplay();
  
  // Check for alerts
  checkMoistureAlert();
  
  // Check Telegram messages
  if (millis() - lastTimeBotRan > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    
    if (numNewMessages) {
      handleNewMessages(numNewMessages);
    }
    
    lastTimeBotRan = millis();
  }
  
  delay(500);
}
