// XIAO ESP32-S3 Complete Sun Tracking Robot
// Features: Servo movement, Light tracking, Camera, Plant ID, Telegram control

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include <ESP32Servo.h>
#include "credentials.h"  // WiFi/telegram/plant.id credentials (not pushed to GitHub)

// Hardware pins
#define SERVO_PIN 1   // D0
#define LDR_PIN 2     // D1
#define USER_LED 21   // Onboard LED

// Camera pins (XIAO ESP32S3 Sense - don't change)
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

// Servo calibration
#define SERVO_STOP    95
#define SERVO_FWD     0
#define SERVO_BWD     180

// Travel calibration
#define TOTAL_DISTANCE_CM  30.0
#define TOTAL_TIME_MS      7060
#define MS_PER_CM          (TOTAL_TIME_MS / TOTAL_DISTANCE_CM)  // fixed servo speed (~235 ms/cm)
#define SCAN_STEPS         10
#define JOG_STEP_CM        1.0    // default manual jog distance

// Auto-tracking
#define AUTO_SCAN_INTERVAL 300000  // 5 minutes

Servo myServo;
WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

float trackLength = TOTAL_DISTANCE_CM;  // adjustable via /setlength (flexible track)
float currentPosition = 0.0;
bool autoTrackingEnabled = false;
unsigned long lastAutoScan = 0;
unsigned long lastBotCheck = 0;

// Light preference derived from plant identification
enum LightNeed { LIGHT_UNKNOWN, LIGHT_SHADE, LIGHT_PARTIAL, LIGHT_FULL_SUN };
LightNeed plantLightNeed = LIGHT_UNKNOWN;   // set by identifyPlant()
String identifiedPlant = "";                // last identified plant name

// Map an identified plant name to its light requirement.
// Falls back to LIGHT_PARTIAL (safe for most houseplants) if unknown.
LightNeed classifyLightNeed(String name) {
  String n = name;
  n.toLowerCase();

  // --- Full sun plants ---
  const char* fullSun[] = {
    "succulent", "cactus", "cacti", "aloe", "echeveria", "sedum", "agave",
    "rosemary", "basil", "thyme", "lavender", "tomato", "pepper", "sunflower",
    "geranium", "hibiscus", "citrus", "jade", "kalanchoe", "portulaca"
  };
  for (unsigned int i = 0; i < sizeof(fullSun) / sizeof(fullSun[0]); i++) {
    if (n.indexOf(fullSun[i]) >= 0) return LIGHT_FULL_SUN;
  }

  // --- Shade / low-light plants ---
  const char* shade[] = {
    "fern", "snake plant", "sansevieria", "zz plant", "zamioculcas",
    "pothos", "philodendron", "peace lily", "spathiphyllum", "calathea",
    "maranta", "aglaonema", "dracaena", "ivy", "moss", "orchid"
  };
  for (unsigned int i = 0; i < sizeof(shade) / sizeof(shade[0]); i++) {
    if (n.indexOf(shade[i]) >= 0) return LIGHT_SHADE;
  }

  // --- Partial / bright indirect light plants ---
  const char* partial[] = {
    "monstera", "ficus", "rubber", "fittonia", "begonia", "coleus",
    "african violet", "saintpaulia", "anthurium", "schefflera",
    "spider plant", "chlorophytum", "croton", "hoya",
    "zinnia"
  };
  for (unsigned int i = 0; i < sizeof(partial) / sizeof(partial[0]); i++) {
    if (n.indexOf(partial[i]) >= 0) return LIGHT_PARTIAL;
  }

  return LIGHT_UNKNOWN;
}

// Human-readable label for a light need
String lightNeedLabel(LightNeed need) {
  switch (need) {
    case LIGHT_FULL_SUN: return "Full sun (brightest spot)";
    case LIGHT_PARTIAL:  return "Bright indirect / partial (medium spot)";
    case LIGHT_SHADE:    return "Shade / low light (dimmest spot)";
    default:             return "Unknown (defaulting to partial)";
  }
}

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // PSRAM check first - determines max resolution we can use
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_UXGA;  // 1600x1200 - maximum detail
    config.jpeg_quality = 8;               // 0-63, lower = better quality
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    config.grab_mode    = CAMERA_GRAB_LATEST;  // always return newest frame (no stale buffers)
    Serial.println("PSRAM found - using UXGA resolution");
  } else {
    config.frame_size   = FRAMESIZE_QVGA;  // 320x240 fallback
    config.jpeg_quality = 10;
    config.fb_count     = 1;
    Serial.println("No PSRAM - using QVGA resolution");
  }

  // try to init camera with retry
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x, retrying...\n", err);
    delay(1000);
    err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed again: 0x%x\n", err);
      Serial.println("Check ribbon cable connection!");
      return;
    }
  }

  Serial.println("Camera OK");

  // tweak sensor settings for better image quality
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    // --- Exposure: photos were washed-out/overexposed, so pull it down ---
    s->set_brightness(s, 2);         // -2 to 2, max brighter for dim rooms
    s->set_contrast(s, 1);           // -2 to 2
    s->set_saturation(s, 1);         // boost color (fixes washed-out look)
    s->set_sharpness(s, 1);          // sharper edges

    s->set_exposure_ctrl(s, 1);      // auto exposure on
    s->set_aec2(s, 1);               // AEC DSP on
    s->set_ae_level(s, 2);           // -2 to 2, max exposure target (brighter)

    s->set_gain_ctrl(s, 1);          // auto gain on
    s->set_agc_gain(s, 0);           // reset gain
    s->set_gainceiling(s, GAINCEILING_16X);  // allow more gain in low light

    // --- White balance: fixes the green/white color cast ---
    s->set_whitebal(s, 1);           // auto white balance on
    s->set_awb_gain(s, 1);           // AWB gain on
    s->set_wb_mode(s, 0);            // 0=auto (try 1=sunny if indoors still green)

    s->set_lenc(s, 1);               // lens correction (evens out brightness)
    s->set_bpc(s, 1);                // black pixel correction
    s->set_wpc(s, 1);                // white pixel correction
    Serial.println("Sensor settings applied");
  }

  // discard first few frames - camera needs time to adjust exposure
  Serial.println("Warming up camera...");
  for (int i = 0; i < 5; i++) {
    camera_fb_t* warmup = esp_camera_fb_get();
    if (warmup) {
      esp_camera_fb_return(warmup);
    }
    delay(100);
  }
  Serial.println("Camera ready");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Sun Tracking Robot with Camera ===");
  
  pinMode(USER_LED, OUTPUT);
  digitalWrite(USER_LED, LOW);
  
  // Setup camera
  setupCamera();
  
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
  }
  
  // Setup Telegram
  client.setInsecure();
  
  // Send startup message
  String msg = "🌞🌿 Sun Tracker + Plant ID Online!\n\n";
  msg += "Movement Commands:\n";
  msg += "/scan - Scan track & show light values\n";
  msg += "/graph - Light bar chart across track\n";
  msg += "/track - Find & move to brightest spot\n";
  msg += "/auto - Auto-track every 5 min\n";
  msg += "/stop - Stop auto-tracking\n";
  msg += "/home - Go to start (0cm)\n";
  msg += "/end - Go to end of track\n";
  msg += "/forward [cm] - Jog forward (default 1cm)\n";
  msg += "/backward [cm] - Jog backward (default 1cm)\n";
  msg += "/setlength <cm> - Set track length\n";
  msg += "/status - Current position & light\n";
  msg += "/reset - Reset position to 0cm\n\n";
  msg += "Camera Commands:\n";
  msg += "/photo - Take & send photo\n";
  msg += "/identify - Identify plant with camera\n";
  msg += "/health - Check plant health\n";
  msg += "/care - Position for identified plant\n";
  msg += "/autocare - Identify + position in one step\n\n";
  msg += "Camera Tuning:\n";
  msg += "/night [0-1200] - Manual exposure (dim rooms)\n";
  msg += "/dayexp - Back to auto exposure\n";
  msg += "/setexp <-2..2> - Exposure level\n";
  msg += "/setwb <0-4> - White balance mode\n\n";
  msg += "Tip: send /menu for tappable buttons.";
  
  bot.sendMessage(chatID, msg, "");
  Serial.println("Telegram bot ready!");
}

void loop() {
  // Check for Telegram messages
  if (millis() - lastBotCheck > 1500) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    
    if (numNewMessages) {
      handleMessages(numNewMessages);
    }
    
    lastBotCheck = millis();
  }
  
  // Auto-tracking mode
  if (autoTrackingEnabled) {
    if (millis() - lastAutoScan >= AUTO_SCAN_INTERVAL) {
      bot.sendMessage(chatID, "🔄 Auto-scan starting...", "");
      performTrack();
      lastAutoScan = millis();
    }
  }
  
  delay(100);
}

// Sends a grid of inline buttons. Each button's callback_data is a command
// string, so taps are routed by the same logic as typed commands.
void sendMenu(String chat_id) {
  String keyboard = "[";
  keyboard += "[{\"text\":\"📸 Photo\",\"callback_data\":\"/photo\"},";
  keyboard +=  "{\"text\":\"🔎 Identify\",\"callback_data\":\"/identify\"}],";
  keyboard += "[{\"text\":\"🩺 Health\",\"callback_data\":\"/health\"},";
  keyboard +=  "{\"text\":\"🌿 Care\",\"callback_data\":\"/care\"}],";
  keyboard += "[{\"text\":\"☀️ Track\",\"callback_data\":\"/track\"},";
  keyboard +=  "{\"text\":\"📊 Graph\",\"callback_data\":\"/graph\"}],";
  keyboard += "[{\"text\":\"🏠 Home\",\"callback_data\":\"/home\"},";
  keyboard +=  "{\"text\":\"🎯 End\",\"callback_data\":\"/end\"}],";
  keyboard += "[{\"text\":\"⬅️ Back 1cm\",\"callback_data\":\"/backward\"},";
  keyboard +=  "{\"text\":\"➡️ Fwd 1cm\",\"callback_data\":\"/forward\"}],";
  keyboard += "[{\"text\":\"📍 Status\",\"callback_data\":\"/status\"}]";
  keyboard += "]";

  bot.sendMessageWithInlineKeyboard(chat_id, "Tap a command:", "", keyboard);
}

void handleMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    
    Serial.println("Received: " + text);
    
    // If this came from an inline button tap, stop the button's spinner.
    // The callback_data holds the command, so routing below still works.
    if (bot.messages[i].type == "callback_query") {
      bot.answerCallbackQuery(bot.messages[i].query_id);
    }
    
    if (text == "/start" || text == "/help" || text == "/menu") {
      String msg = "🌞🌿 Sun Tracker Commands:\n\n";
      msg += "Movement:\n";
      msg += "/scan /graph /track /auto /stop\n";
      msg += "/home /end /status /reset\n";
      msg += "/forward [cm] /backward [cm]\n";
      msg += "/setlength <cm>\n\n";
      msg += "Camera:\n";
      msg += "/photo - Take photo\n";
      msg += "/identify - ID plant\n";
      msg += "/health - Check plant health\n";
      msg += "/care - Position for ID'd plant\n";
      msg += "/autocare - Identify + position\n\n";
      msg += "Tuning:\n";
      msg += "/night [0-1200] /dayexp\n";
      msg += "/setexp <-2..2> /setwb <0-4>";
      bot.sendMessage(chat_id, msg, "");
      sendMenu(chat_id);  // tappable buttons
    }
    else if (text == "/scan") {
      performScan(chat_id);
    }
    else if (text == "/graph") {
      performGraph(chat_id);
    }
    else if (text == "/track") {
      performTrack();
    }
    else if (text == "/auto") {
      autoTrackingEnabled = true;
      lastAutoScan = millis();
      bot.sendMessage(chat_id, "✅ Auto-tracking ENABLED\nScanning every 5 minutes", "");
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
      bot.sendMessage(chat_id, "🎯 Going to END (" + String(trackLength, 1) + "cm)...", "");
      goToEnd();
      bot.sendMessage(chat_id, "✅ At end position", "");
    }
    else if (text == "/forward" || text.startsWith("/forward ")) {
      float step = JOG_STEP_CM;
      if (text.length() > 9) step = text.substring(9).toFloat();
      moveDistance(step);
      bot.sendMessage(chat_id, "➡️ Moved forward. Position: " + String(currentPosition, 1) + " cm", "");
    }
    else if (text == "/backward" || text.startsWith("/backward ")) {
      float step = JOG_STEP_CM;
      if (text.length() > 10) step = text.substring(10).toFloat();
      moveDistance(-step);
      bot.sendMessage(chat_id, "⬅️ Moved backward. Position: " + String(currentPosition, 1) + " cm", "");
    }
    else if (text.startsWith("/setlength ")) {
      float newLen = text.substring(11).toFloat();
      if (newLen >= 1.0 && newLen <= 100.0) {
        trackLength = newLen;
        if (currentPosition > trackLength) currentPosition = trackLength;
        bot.sendMessage(chat_id, "📏 Track length set to " + String(trackLength, 1) + " cm", "");
      } else {
        bot.sendMessage(chat_id, "⚠️ Invalid length. Use 1-100 cm, e.g. /setlength 20", "");
      }
    }
    else if (text == "/status") {
      int light = readLight();
      String msg = "📊 Status:\n\n";
      msg += "Position: " + String(currentPosition, 1) + " cm\n";
      msg += "Track length: " + String(trackLength, 1) + " cm\n";
      msg += "Light level: " + String(light) + "\n";
      msg += "Auto-tracking: " + String(autoTrackingEnabled ? "ON" : "OFF");
      bot.sendMessage(chat_id, msg, "");
    }
    else if (text == "/reset") {
      currentPosition = 0.0;
      bot.sendMessage(chat_id, "🔄 Position reset to 0cm (home)", "");
    }
    else if (text == "/photo") {
      handlePhoto(chat_id);
    }
    else if (text == "/identify") {
      handleIdentify(chat_id);
    }
    else if (text == "/health") {
      handleHealth(chat_id);
    }
    else if (text == "/care") {
      performCareTrack(chat_id);
    }
    else if (text == "/autocare") {
      handleIdentify(chat_id);
      performCareTrack(chat_id);
    }
    else if (text.startsWith("/setexp ")) {
      int lvl = constrain(text.substring(8).toInt(), -2, 2);
      sensor_t* s = esp_camera_sensor_get();
      if (s) {
        s->set_exposure_ctrl(s, 1);   // ensure auto exposure on
        s->set_ae_level(s, lvl);
        s->set_brightness(s, lvl);    // match brightness to exposure request
        bot.sendMessage(chat_id, "🔆 Exposure level set to " + String(lvl) + " (-2 dark .. 2 bright). Take /photo to check.", "");
      }
    }
    else if (text == "/night" || text.startsWith("/night ")) {
      // Force a long manual exposure for very dim rooms
      int val = 1200;  // 0-1200, higher = brighter/longer exposure
      if (text.length() > 7) val = constrain(text.substring(7).toInt(), 0, 1200);
      sensor_t* s = esp_camera_sensor_get();
      if (s) {
        s->set_exposure_ctrl(s, 0);   // turn OFF auto exposure
        s->set_aec_value(s, val);     // manual exposure time
        bot.sendMessage(chat_id, "🌙 Manual night mode: exposure=" + String(val) + "/1200.\nSend /night <0-1200> to adjust, /dayexp for auto.", "");
      }
    }
    else if (text == "/dayexp") {
      sensor_t* s = esp_camera_sensor_get();
      if (s) {
        s->set_exposure_ctrl(s, 1);   // back to auto exposure
        s->set_aec2(s, 1);
        bot.sendMessage(chat_id, "☀️ Auto exposure re-enabled.", "");
      }
    }
    else if (text.startsWith("/setwb ")) {
      int mode = constrain(text.substring(7).toInt(), 0, 4);
      sensor_t* s = esp_camera_sensor_get();
      if (s) {
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, mode);      // 0 auto,1 sunny,2 cloudy,3 office,4 home
        bot.sendMessage(chat_id, "🎨 White-balance mode " + String(mode) + " (0 auto,1 sunny,2 cloudy,3 office,4 home). Take /photo.", "");
      }
    }
  }
}

void performScan(String chat_id) {
  bot.sendMessage(chat_id, "🔍 Starting scan...", "");
  
  goHome();
  delay(500);
  
  String results = "📊 Scan Results:\n\n";
  float stepSize = trackLength / (SCAN_STEPS - 1);
  
  for (int i = 0; i < SCAN_STEPS; i++) {
    float targetPos = i * stepSize;
    
    float distance = targetPos - currentPosition;
    if (abs(distance) > 0.1) {
      moveDistance(distance);
      delay(500);
    }
    
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

void performGraph(String chat_id) {
  bot.sendMessage(chat_id, "📈 Building light map...", "");

  goHome();
  delay(500);

  float stepSize = trackLength / (SCAN_STEPS - 1);
  int values[SCAN_STEPS];
  int maxVal = 1;
  int brightestIdx = 0;

  // Scan all positions and store readings
  for (int i = 0; i < SCAN_STEPS; i++) {
    float targetPos = i * stepSize;
    float distance = targetPos - currentPosition;
    if (abs(distance) > 0.1) {
      moveDistance(distance);
      delay(500);
    }
    int lightValue = readLight();
    values[i] = lightValue;
    if (lightValue > maxVal) {
      maxVal = lightValue;
      brightestIdx = i;
    }
  }

  // Build ASCII bar chart (scaled to brightest reading)
  String graph = "📊 Light Map (0-30cm):\n\n";
  for (int i = 0; i < SCAN_STEPS; i++) {
    float pos = i * stepSize;
    int bars = (int)((values[i] / (float)maxVal) * 20);
    graph += String(pos, 0);
    if (pos < 10) graph += " ";  // align single digits
    graph += "cm |";
    for (int j = 0; j < bars; j++) {
      graph += "█";
    }
    graph += " " + String(values[i]);
    if (i == brightestIdx) graph += " ☀️";
    graph += "\n";
  }
  graph += "\nBrightest: " + String(brightestIdx * stepSize, 1) + "cm (" + String(maxVal) + ")";

  bot.sendMessage(chat_id, graph, "");
  goHome();
}

void performTrack() {
  Serial.println("=== Tracking brightest spot ===");
  
  goHome();
  delay(500);
  
  float stepSize = trackLength / (SCAN_STEPS - 1);
  float brightestPosition = 0.0;
  int brightestValue = 0;
  int darkestValue = 4095;
  
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
    if (lightValue < darkestValue) {
      darkestValue = lightValue;
    }
    
    Serial.print(currentPosition, 1);
    Serial.print("cm: ");
    Serial.println(lightValue);
  }
  
  // If readings barely differ, the sensor has no usable contrast
  int spread = brightestValue - darkestValue;
  if (spread < 100) {
    goHome();
    String warn = "⚠️ Can't track - no light difference!\n\n";
    warn += "All readings ~" + String(brightestValue) + " (spread only " + String(spread) + ")\n\n";
    warn += "The LDR is saturated. Fix the sensor:\n";
    warn += "- Use a smaller series resistor, or\n";
    warn += "- Swap the LDR/resistor positions\n";
    warn += "Then /track will work.";
    bot.sendMessage(chatID, warn, "");
    return;
  }
  
  moveToPosition(brightestPosition);
  
  String msg = "☀️ Moved to brightest spot!\n\n";
  msg += "Position: " + String(brightestPosition, 1) + " cm\n";
  msg += "Light level: " + String(brightestValue) + "\n";
  msg += "(darkest was " + String(darkestValue) + ")";
  bot.sendMessage(chatID, msg, "");
}

// Scans the track and positions the plant according to its identified light need:
//   full sun  -> brightest spot
//   shade     -> dimmest spot
//   partial   -> spot closest to the mid brightness level
void performCareTrack(String chat_id) {
  if (plantLightNeed == LIGHT_UNKNOWN && identifiedPlant.length() == 0) {
    bot.sendMessage(chat_id, "🤔 No plant identified yet.\nSend /identify first, then /care.", "");
    return;
  }

  LightNeed need = plantLightNeed;
  bool assumedPartial = (need == LIGHT_UNKNOWN);
  if (assumedPartial) need = LIGHT_PARTIAL;  // safe default for unknown plants

  String intro = "🌿 Caring for: " + (identifiedPlant.length() ? identifiedPlant : String("plant")) + "\n";
  intro += "Target: " + lightNeedLabel(need);
  if (assumedPartial) intro += " (assumed)";
  bot.sendMessage(chat_id, intro, "");

  goHome();
  delay(500);

  float stepSize = trackLength / (SCAN_STEPS - 1);
  int values[SCAN_STEPS];
  int maxVal = 0, minVal = 4095;

  for (int i = 0; i < SCAN_STEPS; i++) {
    float targetPos = i * stepSize;
    float distance = targetPos - currentPosition;
    if (abs(distance) > 0.1) {
      moveDistance(distance);
      delay(500);
    }
    int lightValue = readLight();
    values[i] = lightValue;
    if (lightValue > maxVal) maxVal = lightValue;
    if (lightValue < minVal) minVal = lightValue;
  }

  int spread = maxVal - minVal;
  if (spread < 100) {
    goHome();
    bot.sendMessage(chat_id, "⚠️ Can't position - no light difference across the track (spread " + String(spread) + "). Check the LDR sensor.", "");
    return;
  }

  // Pick the target brightness for this need
  int target;
  if (need == LIGHT_FULL_SUN)      target = maxVal;
  else if (need == LIGHT_SHADE)    target = minVal;
  else                             target = (maxVal + minVal) / 2;  // partial

  // Find the position whose reading is closest to the target
  int bestIdx = 0;
  int bestDiff = 999999;
  for (int i = 0; i < SCAN_STEPS; i++) {
    int diff = abs(values[i] - target);
    if (diff < bestDiff) {
      bestDiff = diff;
      bestIdx = i;
    }
  }

  float bestPos = bestIdx * stepSize;
  moveToPosition(bestPos);

  String msg = "✅ Positioned for " + lightNeedLabel(need) + "\n\n";
  msg += "Plant: " + (identifiedPlant.length() ? identifiedPlant : String("(unknown)")) + "\n";
  msg += "Position: " + String(bestPos, 1) + " cm\n";
  msg += "Light level: " + String(values[bestIdx]) + "\n";
  msg += "(track range " + String(minVal) + "-" + String(maxVal) + ")";
  bot.sendMessage(chat_id, msg, "");
}

// Global variables for photo sending callbacks
static camera_fb_t* currentFb = nullptr;
static size_t currentIndex = 0;

bool isMoreDataAvailable() {
  return currentIndex < currentFb->len;
}

uint8_t getNextByte() {
  return currentFb->buf[currentIndex++];
}

void handlePhoto(String chat_id) {
  bot.sendMessage(chat_id, "📸 Taking photo...", "");
  
  digitalWrite(USER_LED, HIGH);
  delay(300);
  
  camera_fb_t* fb = capturePhoto();
  digitalWrite(USER_LED, LOW);
  
  if (!fb) {
    bot.sendMessage(chat_id, "❌ Camera failed - try again", "");
    return;
  }
  
  Serial.printf("Photo captured: %d bytes\n", fb->len);
  
  // Set up callbacks
  currentFb = fb;
  currentIndex = 0;
  
  // Send photo to Telegram
  String sent = bot.sendPhotoByBinary(chat_id, "image/jpeg", fb->len,
                                       isMoreDataAvailable, getNextByte,
                                       nullptr, nullptr);
  
  esp_camera_fb_return(fb);
  currentFb = nullptr;
  
  if (sent) {
    Serial.println("Photo sent to Telegram");
  } else {
    bot.sendMessage(chat_id, "❌ Failed to send photo", "");
  }
}

void handleIdentify(String chat_id) {
  bot.sendMessage(chat_id, "📸 Taking photo...", "");
  
  digitalWrite(USER_LED, HIGH);
  delay(300);
  
  camera_fb_t* fb = capturePhoto();
  digitalWrite(USER_LED, LOW);
  
  if (!fb) {
    bot.sendMessage(chat_id, "❌ Camera failed - try again", "");
    return;
  }
  
  Serial.printf("Photo captured: %d bytes\n", fb->len);
  
  // Send photo first
  currentFb = fb;
  currentIndex = 0;
  bot.sendPhotoByBinary(chat_id, "image/jpeg", fb->len,
                        isMoreDataAvailable, getNextByte,
                        nullptr, nullptr);
  
  bot.sendMessage(chat_id, "🔍 Identifying plant...", "");
  
  String result = identifyPlant(fb);
  esp_camera_fb_return(fb);
  currentFb = nullptr;
  
  if (result.length() == 0) {
    bot.sendMessage(chat_id, "❌ No result from API", "");
    return;
  }
  
  String msg = "🌿 Plant ID Result:\n\n" + result;
  bot.sendMessage(chat_id, msg, "");
}

void handleHealth(String chat_id) {
  bot.sendMessage(chat_id, "📸 Taking photo...", "");
  
  digitalWrite(USER_LED, HIGH);
  delay(300);
  
  // Discard stale/blurry first frame, keep the latest
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb) { esp_camera_fb_return(fb); }
  delay(100);
  fb = esp_camera_fb_get();
  digitalWrite(USER_LED, LOW);
  
  if (!fb) {
    bot.sendMessage(chat_id, "❌ Camera failed - try again", "");
    return;
  }
  
  Serial.printf("Photo captured: %d bytes\n", fb->len);
  
  // Send photo first
  currentFb = fb;
  currentIndex = 0;
  bot.sendPhotoByBinary(chat_id, "image/jpeg", fb->len,
                        isMoreDataAvailable, getNextByte,
                        nullptr, nullptr);
  
  bot.sendMessage(chat_id, "🩺 Checking plant health...", "");
  
  String result = checkPlantHealth(fb->buf, fb->len);
  esp_camera_fb_return(fb);
  currentFb = nullptr;
  
  if (result.length() == 0) {
    bot.sendMessage(chat_id, "❌ No result from API", "");
    return;
  }
  
  bot.sendMessage(chat_id, result, "");
}

// Checks plant health - diseases, pests, nutritional issues
String checkPlantHealth(uint8_t* imgBuf, size_t imgLen) {
  Serial.println("Checking plant health...");
  
  size_t encodedLen = ((imgLen + 2) / 3) * 4 + 1;
  unsigned char* encoded = (unsigned char*)malloc(encodedLen);
  if (!encoded) {
    return "Memory error - restart device";
  }
  
  size_t outLen = 0;
  mbedtls_base64_encode(encoded, encodedLen, &outLen, imgBuf, imgLen);
  String b64image = String((char*)encoded);
  free(encoded);
  
  // Health assessment API endpoint
  String body = "{\"images\":[\"data:image/jpg;base64,";
  body += b64image;
  body += "\"],\"modifiers\":[\"disease_similar_images\"],";
  body += "\"disease_details\":[\"cause\",\"common_names\",\"treatment\"]}";
  
  WiFiClientSecure apiClient;
  apiClient.setInsecure();
  
  HTTPClient http;
  http.begin(apiClient, "https://api.plant.id/v2/health_assessment");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Api-Key", plantIdApiKey);
  http.setTimeout(15000);
  
  int code = http.POST(body);
  Serial.printf("Health API response: %d\n", code);
  
  String result = "";
  
  if (code == 200) {
    String response = http.getString();
    Serial.println("Health API raw response:");
    Serial.println(response);

    DynamicJsonDocument doc(16384);
    DeserializationError err = deserializeJson(doc, response);
    
    if (err) {
      result = "Failed to parse response";
    } else {
      // v2/health_assessment nests results under "health_assessment"
      JsonObject ha = doc["health_assessment"].as<JsonObject>();
      if (ha.isNull()) {
        ha = doc.as<JsonObject>();  // fallback if returned at top level
      }

      bool hasProb = ha.containsKey("is_healthy_probability");
      bool isHealthy = ha["is_healthy"].as<bool>();
      float healthyProb = ha["is_healthy_probability"].as<float>();
      
      if (!hasProb) {
        result = "⚠️ Unable to analyze plant health\n\n";
        result += "Try:\n";
        result += "1. Twist camera lens to focus\n";
        result += "2. Get closer (10-30cm)\n";
        result += "3. Use good lighting\n";
        result += "4. Take /photo first to check quality";
      }
      else if (isHealthy) {
        result = "✅ Plant looks healthy!\n";
        result += "Confidence: " + String(healthyProb * 100, 1) + "%\n\n";
        result += "No diseases or pests detected.";
      } else {
        result = "⚠️ Potential issues detected:\n";
        result += "Healthy probability: " + String(healthyProb * 100, 1) + "%\n\n";
        int numDiseases = ha["diseases"].size();
        if (numDiseases > 0) {
          for (int i = 0; i < min(3, numDiseases); i++) {
            String name = ha["diseases"][i]["name"].as<String>();
            float prob = ha["diseases"][i]["probability"].as<float>();
            result += String(i + 1) + ". " + name + "\n";
            result += "   Probability: " + String(prob * 100, 0) + "%\n";
            if (ha["diseases"][i]["disease_details"]["treatment"]["chemical"].size() > 0) {
              String treatment = ha["diseases"][i]["disease_details"]["treatment"]["chemical"][0].as<String>();
              if (treatment.length() > 0 && treatment.length() < 100) {
                result += "   Treatment: " + treatment + "\n";
              }
            }
            result += "\n";
          }
        } else {
          result += "No specific diseases identified.\n";
          result += "Plant may be stressed but no clear disease.";
        }
      }
    }
  } else if (code == 401) {
    result = "❌ Invalid API key";
  } else if (code == 429) {
    result = "⚠️ Daily limit reached (100/day)";
  } else {
    result = "API error: " + String(code);
  }
  
  http.end();
  return result;
}

String identifyPlant(camera_fb_t* fb) {
  Serial.println("Encoding image...");
  
  size_t encodedLen = ((fb->len + 2) / 3) * 4 + 1;
  unsigned char* encoded = (unsigned char*)malloc(encodedLen);
  
  if (!encoded) {
    return "Memory error - restart device";
  }
  
  size_t outLen = 0;
  mbedtls_base64_encode(encoded, encodedLen, &outLen, fb->buf, fb->len);
  
  String b64image = String((char*)encoded);
  free(encoded);
  
  String body = "{\"images\":[\"data:image/jpg;base64,";
  body += b64image;
  body += "\"],\"modifiers\":[\"crops_fast\"],\"plant_language\":\"en\",";
  body += "\"plant_details\":[\"common_names\"]}";
  
  WiFiClientSecure apiClient;
  apiClient.setInsecure();
  
  HTTPClient http;
  http.begin(apiClient, "https://api.plant.id/v2/identify");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Api-Key", plantIdApiKey);
  http.setTimeout(15000);
  
  int code = http.POST(body);
  Serial.printf("API response: %d\n", code);
  
  String result = "";
  
  if (code == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, response);
    
    if (!err) {
      int numSuggestions = doc["suggestions"].size();
      
      if (numSuggestions > 0) {
        String topName = doc["suggestions"][0]["plant_name"].as<String>();
        float topProb = doc["suggestions"][0]["probability"].as<float>();
        String common = "";
        
        if (doc["suggestions"][0]["plant_details"]["common_names"].size() > 0) {
          common = doc["suggestions"][0]["plant_details"]["common_names"][0].as<String>();
        }
        
        result += "Plant: " + topName + "\n";
        if (common.length() > 0) {
          result += "Common: " + common + "\n";
        }
        result += "Confidence: " + String(topProb * 100, 1) + "%\n";
        
        int showCount = min(3, numSuggestions);
        result += "\nTop " + String(showCount) + ":\n";
        for (int i = 0; i < showCount; i++) {
          String n = doc["suggestions"][i]["plant_name"].as<String>();
          float p = doc["suggestions"][i]["probability"].as<float>();
          result += String(i + 1) + ". " + n + " - " + String(p * 100, 0) + "%\n";
        }

        // Determine light preference from both scientific and common names
        identifiedPlant = topName;
        plantLightNeed = classifyLightNeed(topName);
        if (plantLightNeed == LIGHT_UNKNOWN && common.length() > 0) {
          plantLightNeed = classifyLightNeed(common);
        }
        result += "\n☀️ Light need: " + lightNeedLabel(plantLightNeed) + "\n";
        result += "Send /care to auto-position for this plant.";
      } else {
        result = "No plant found - try clearer photo";
      }
    } else {
      result = "JSON parse error";
    }
  } else if (code == 401) {
    result = "Invalid API key";
  } else if (code == 429) {
    result = "Daily limit reached (100/day)";
  } else {
    result = "API error: " + String(code);
  }
  
  http.end();
  return result;
}

void goHome() {
  moveToPosition(0.0);
}

void goToEnd() {
  moveToPosition(trackLength);
}

void moveToPosition(float targetCm) {
  targetCm = constrain(targetCm, 0, trackLength);
  float distance = targetCm - currentPosition;
  moveDistance(distance);
}

void moveDistance(float distanceCm) {
  // Clamp so we don't drive past the track ends
  float target = constrain(currentPosition + distanceCm, 0, trackLength);
  distanceCm = target - currentPosition;
  if (abs(distanceCm) < 0.1) return;
  
  float timeMs = abs(distanceCm) * MS_PER_CM;  // fixed speed, independent of track length
  
  if (distanceCm > 0) {
    myServo.write(SERVO_FWD);
  } else {
    myServo.write(SERVO_BWD);
  }
  
  delay((int)timeMs);
  myServo.write(SERVO_STOP);
  
  currentPosition += distanceCm;
  currentPosition = constrain(currentPosition, 0, trackLength);
}

int readLight() {
  int raw = analogRead(LDR_PIN);
  int mv = analogReadMilliVolts(LDR_PIN);  // calibrated voltage at the pin
  Serial.printf("LDR raw: %d | voltage: %d mV\n", raw, mv);
  return raw;
}

// Takes a proper photo with warm-up frames to fix blur and stale-frame issues
camera_fb_t* capturePhoto() {
  // discard several frames so the sensor adjusts exposure/gain/focus
  for (int i = 0; i < 4; i++) {
    camera_fb_t* discard = esp_camera_fb_get();
    if (discard) {
      esp_camera_fb_return(discard);
    }
    delay(120);
  }
  // now grab the real (fresh) frame
  return esp_camera_fb_get();
}
