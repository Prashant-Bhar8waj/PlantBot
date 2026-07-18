// XIAO ESP32-S3 Camera + Servo Control via Telegram
// Rack and pinion mechanism with camera on stick
// Adjust TRAVEL_TIME_MS to calibrate full 14cm travel

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP32Servo.h>
#include "esp_camera.h"
#include "credentials.h"  // WiFi/telegram credentials (not pushed to GitHub)

// Servo setup
Servo myServo;
#define SERVO_PIN 1  // D0 (GPIO 1) on XIAO ESP32-S3

// Camera pins for XIAO ESP32-S3 Sense
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// Continuous rotation servo settings
// Neutral 110: above 110 = forward, below 110 = backward
#define SERVO_STOP     110  // neutral (calibrated)
#define SERVO_FWD      140  // forward speed (above neutral)
#define SERVO_BWD      80   // backward speed (below neutral)

// Time to travel full 14cm — CALIBRATED
// 15cm in 2s = 7.5cm/s, so 14cm = 1.87s
#define TRAVEL_TIME_MS  1870  // ms to travel full 14cm forward
#define SCAN_POSITIONS  5     // number of photos during scan

unsigned long travelStartTime = 0;
float estimatedPosition = 0;  // 0.0 to 1.0 (fraction of total travel)

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

unsigned long lastBotCheck = 0;
bool cameraInitialized = false;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== XIAO ESP32-S3 Camera + Servo ===");
  
  // Setup continuous rotation servo
  Serial.println("Initializing continuous servo on GPIO 1...");
  myServo.attach(SERVO_PIN, 500, 2400);
  
  // Make sure it's stopped
  myServo.write(SERVO_STOP);
  delay(500);
  Serial.println("✓ Servo stopped at center");
  
  // Quick test: spin forward 1s then backward 1s
  Serial.println("Testing servo FWD (120) for 1s...");
  myServo.write(120);
  delay(1000);
  myServo.write(SERVO_STOP);
  delay(300);
  Serial.println("Testing servo BWD (60) for 1s...");
  myServo.write(60);
  delay(1000);
  myServo.write(SERVO_STOP);
  Serial.println("✓ Servo test done. Check if it moved!");
  
  estimatedPosition = 0;
  Serial.println("✓ Ready at start position");
  
  // Initialize camera
  if (initCamera()) {
    Serial.println("Camera initialized successfully");
    cameraInitialized = true;
  } else {
    Serial.println("Camera initialization failed!");
  }
  
  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    // Try reconnecting every 10 attempts
    if (attempts % 10 == 0) {
      Serial.println("\nRetrying WiFi connection...");
      WiFi.disconnect();
      delay(100);
      WiFi.begin(ssid, password);
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\n✗ WiFi connection FAILED!");
    Serial.println("Please check:");
    Serial.println("1. WiFi name (SSID): " + String(ssid));
    Serial.println("2. WiFi password is correct");
    Serial.println("3. WiFi is 2.4GHz (ESP32 doesn't support 5GHz)");
    Serial.println("4. You're in range of the WiFi");
    Serial.println("\nBot will NOT work without WiFi!");
  }
  
  // Configure secure client for Telegram
  client.setInsecure();
  
  // Test Telegram connection
  Serial.println("Testing Telegram connection...");
  String msg = "🤖 Plant Camera Bot Online!\n\n";
  msg += "Camera: " + String(cameraInitialized ? "OK" : "FAILED") + "\n";
  msg += "Stick length: 14.5cm\n";
  msg += "Travel distance: ~140mm (time-based)\n\n";
  msg += "Commands:\n";
  msg += "/photo - Take photo at current position\n";
  msg += "/scan - Extended scan (8 photos)\n";
  msg += "/sweep - Full 14cm sweep + back\n";
  msg += "/fwd - Move full forward (~14cm)\n";
  msg += "/back - Move full backward\n";
  msg += "/move <0-100> - Move to position (%)\n";
  msg += "/home - Return to start (0°)\n";
  msg += "/end - Move to end (180°)\n";
  msg += "/status - Show current position";
  
  if (bot.sendMessage(chatID, msg, "")) {
    Serial.println("✓ Telegram connected! Bot ready!");
  } else {
    Serial.println("✗ Telegram connection failed!");
    Serial.println("Check bot token and chat ID");
  }
  
  delay(1000);
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Attempting reconnection...");
    WiFi.disconnect();
    delay(1000);
    WiFi.begin(ssid, password);
    
    int reconnectAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && reconnectAttempts < 20) {
      delay(500);
      Serial.print(".");
      reconnectAttempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n✓ WiFi reconnected!");
    } else {
      Serial.println("\n✗ WiFi reconnection failed. Waiting 10s...");
      delay(10000);
    }
    return;
  }
  
  // Check for Telegram messages
  if (millis() - lastBotCheck > 1000) {
    int numNew = bot.getUpdates(bot.last_message_received + 1);
    
    if (numNew) {
      Serial.print("Received ");
      Serial.print(numNew);
      Serial.println(" new message(s)");
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
      String msg = "🌱 Plant Camera Bot\n\n";
      msg += "Control camera position and capture photos\n\n";
      msg += "Commands:\n";
      msg += "/photo - Take photo\n";
      msg += "/scan - Auto scan (4 photos)\n";
      msg += "/move <angle> - Move servo\n";
      msg += "/home - Go to start\n";
      msg += "/end - Go to end\n";
      msg += "/status - Current position";
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/photo") {
      bot.sendMessage(chat, "📸 Taking photo...", "");
      if (takeAndSendPhoto(chat)) {
        String msg = "✓ Photo captured at ~" + String((int)(estimatedPosition * 140)) + "mm";
        bot.sendMessage(chat, msg, "");
      } else {
        bot.sendMessage(chat, "❌ Photo capture failed", "");
      }
    }
    else if (text == "/scan") {
      bot.sendMessage(chat, "🔍 Starting extended scan...", "");
      performScan(chat);
    }
    else if (text == "/sweep") {
      bot.sendMessage(chat, "🔄 Sweeping full 14cm...", "");
      performSweep(chat);
    }
    else if (text == "/fwd") {
      bot.sendMessage(chat, "⏩ Moving full forward (~14cm)...", "");
      moveFullForward(chat);
    }
    else if (text == "/back") {
      bot.sendMessage(chat, "⏪ Moving full backward...", "");
      moveFullBackward(chat);
    }
    else if (text == "/home") {
      bot.sendMessage(chat, "🏠 Moving to home (0%)...", "");
      moveFullBackward(chat);
    }
    else if (text == "/end") {
      bot.sendMessage(chat, "➡️ Moving to end (100%)...", "");
      moveFullForward(chat);
    }
    else if (text == "/stop") {
      servoStop();
      bot.sendMessage(chat, "⏹ Servo stopped!", "");
    }
    else if (text == "/test") {
      bot.sendMessage(chat, "🔧 Testing servo...", "");
      // Try full speed forward
      myServo.write(180);
      delay(1000);
      myServo.write(SERVO_STOP);
      delay(500);
      // Try full speed backward
      myServo.write(0);
      delay(1000);
      myServo.write(SERVO_STOP);
      bot.sendMessage(chat, "✓ Test done! Did servo move?", "");
    }
    else if (text == "/status") {
      String msg = "📍 Current Position:\n\n";
      msg += "Position: ~" + String((int)(estimatedPosition * 140)) + "mm / 140mm\n";
      msg += "Progress: " + String((int)(estimatedPosition * 100)) + "%\n";
      msg += "Travel time: " + String(TRAVEL_TIME_MS) + "ms";
      bot.sendMessage(chat, msg, "");
    }
    else if (text.startsWith("/move ")) {
      int pct = text.substring(6).toInt();
      if (pct >= 0 && pct <= 100) {
        String msg = "Moving to " + String(pct) + "% (~" + String(pct * 140 / 100) + "mm)...";
        bot.sendMessage(chat, msg, "");
        moveToPosition(pct);
        bot.sendMessage(chat, "✓ Position reached", "");
      } else {
        bot.sendMessage(chat, "Invalid! Use /move 0-100 (percentage)", "");
      }
    }
    else {
      bot.sendMessage(chat, "Unknown command. Try /start", "");
    }
  }
}

void performScan(String chat) {
  // Scan full 14cm length, taking 5 photos evenly spaced
  bot.sendMessage(chat, "🔍 Scanning full 14cm (" + String(SCAN_POSITIONS) + " photos)...", "");
  
  // Start from home
  moveFullBackward(chat);
  delay(500);
  
  // Take photos at evenly spaced positions: 0%, 25%, 50%, 75%, 100%
  for (int i = 0; i < SCAN_POSITIONS; i++) {
    int pct = (i * 100) / (SCAN_POSITIONS - 1);
    int mm  = pct * 140 / 100;
    
    String msg = "📸 Photo " + String(i + 1) + "/" + String(SCAN_POSITIONS) + " at ~" + String(mm) + "mm";
    bot.sendMessage(chat, msg, "");
    
    moveToPosition(pct);
    delay(800);  // Let camera stabilize
    takeAndSendPhoto(chat);
    delay(500);
  }
  
  bot.sendMessage(chat, "✓ Scan complete! Returning home...", "");
  moveFullBackward(chat);
  bot.sendMessage(chat, "✓ Back at home position", "");
}

void performSweep(String chat) {
  // Move forward N revolutions to cover 14cm, then return
  bot.sendMessage(chat, "⏩ Sweeping full 14cm and returning...", "");
  
  moveFullForward(chat);
  delay(1000);
  
  bot.sendMessage(chat, "⏪ Returning to start...", "");
  moveFullBackward(chat);
  
  bot.sendMessage(chat, "✓ Full sweep complete!", "");
}

void moveFullForward(String chat) {
  bot.sendMessage(chat, "⏩ Moving forward (~14cm)...", "");
  Serial.println("Moving full forward");
  
  myServo.write(SERVO_FWD);
  delay(TRAVEL_TIME_MS);
  myServo.write(SERVO_STOP);
  
  estimatedPosition = 1.0;
  bot.sendMessage(chat, "✓ Reached end!", "");
}

void moveFullBackward(String chat) {
  bot.sendMessage(chat, "⏪ Moving backward to home...", "");
  Serial.println("Moving full backward");
  
  myServo.write(SERVO_BWD);
  delay(TRAVEL_TIME_MS + 500);  // +500ms to ensure it reaches home
  myServo.write(SERVO_STOP);
  
  estimatedPosition = 0;
  bot.sendMessage(chat, "✓ Back at home!", "");
}

void servoStop() {
  myServo.write(SERVO_STOP);
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;  // Important for XIAO ESP32S3
  
  // Use smaller frame size for better stability
  config.frame_size = FRAMESIZE_QVGA;  // 320x240
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_DRAM;  // Important for XIAO ESP32S3
  
  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }
  
  // Get camera sensor and adjust settings
  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect)
    s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
    s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
    s->set_wb_mode(s, 0);        // 0 to 4
    s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
    s->set_aec2(s, 0);           // 0 = disable , 1 = enable
    s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
    s->set_agc_gain(s, 0);       // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
    s->set_bpc(s, 0);            // 0 = disable , 1 = enable
    s->set_wpc(s, 1);            // 0 = disable , 1 = enable
    s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
    s->set_lenc(s, 1);           // 0 = disable , 1 = enable
    s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
    s->set_vflip(s, 0);          // 0 = disable , 1 = enable
    s->set_dcw(s, 1);            // 0 = disable , 1 = enable
    s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
  }
  
  Serial.println("Camera initialized successfully!");
  return true;
}

// Global variables for photo buffer (needed for callbacks)
camera_fb_t *currentFrameBuffer = nullptr;
int photoOffset = 0;

// Callback: Check if more data is available (NO parameters)
bool isMoreDataAvailable() {
  return (currentFrameBuffer != nullptr && photoOffset < currentFrameBuffer->len);
}

// Callback: Get next byte (NO parameters)
uint8_t getNextByte() {
  if (currentFrameBuffer == nullptr || photoOffset >= currentFrameBuffer->len) {
    return 0;
  }
  return currentFrameBuffer->buf[photoOffset++];
}

// Callback: Get next buffer chunk (NO parameters, returns pointer)
uint8_t* getNextBuffer() {
  static uint8_t buffer[1024];
  
  if (currentFrameBuffer == nullptr || photoOffset >= currentFrameBuffer->len) {
    return nullptr;
  }
  
  int bytesToCopy = min(1024, (int)(currentFrameBuffer->len - photoOffset));
  memcpy(buffer, currentFrameBuffer->buf + photoOffset, bytesToCopy);
  photoOffset += bytesToCopy;
  
  return buffer;
}

bool takeAndSendPhoto(String chat) {
  if (!cameraInitialized) {
    return false;
  }
  
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return false;
  }
  
  Serial.println("Sending photo to Telegram...");
  
  // Set global variables for callbacks
  currentFrameBuffer = fb;
  photoOffset = 0;
  
  // Send photo using callback functions
  String response = bot.sendMultipartFormDataToTelegram(
    "sendPhoto",
    "photo",
    "photo.jpg",
    "image/jpeg",
    chat,
    fb->len,
    isMoreDataAvailable,
    getNextByte,
    getNextBuffer,
    nullptr
  );
  
  // Clean up
  currentFrameBuffer = nullptr;
  photoOffset = 0;
  esp_camera_fb_return(fb);
  
  bool success = response.indexOf("\"ok\":true") > 0;
  Serial.println(success ? "Photo sent successfully!" : "Photo send failed");
  
  return success;
}

void writeServo(int angle) {
  angle = constrain(angle, 0, 180);
  myServo.write(angle);
}

void moveToPosition(int targetPercent) {
  // targetPercent: 0 = home, 100 = full end
  targetPercent = constrain(targetPercent, 0, 100);
  float target = targetPercent / 100.0;
  
  float diff = target - estimatedPosition;
  long travelMs = abs(diff) * TRAVEL_TIME_MS;
  
  if (travelMs < 50) return;  // already close enough
  
  Serial.print("Moving from ");
  Serial.print((int)(estimatedPosition * 100));
  Serial.print("% to ");
  Serial.print(targetPercent);
  Serial.println("%");
  
  if (diff > 0) {
    myServo.write(SERVO_FWD);
  } else {
    myServo.write(SERVO_BWD);
  }
  
  delay(travelMs);
  myServo.write(SERVO_STOP);
  estimatedPosition = target;
  
  Serial.print("✓ At ~");
  Serial.print(targetPercent);
  Serial.println("%");
}
