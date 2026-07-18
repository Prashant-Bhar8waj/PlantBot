// ESP32-CAM plant identification - STANDALONE VERSION
// All credentials included in this file
// ⚠️ DO NOT PUSH THIS FILE TO GITHUB - use esp32cam_plant_identify.ino instead
//
// takes photo when /identify is sent on telegram
// sends photo to plant.id API and returns plant name
//
// board: Seeed Studio XIAO ESP32S3 Sense
// upload: just plug in USB-C, no FTDI needed

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"

// ========== CREDENTIALS - FILL THESE IN ==========
const char* ssid = "dud2_2";
const char* password = "University283";
const char* botToken = "8931561780:AAGWKCfq83UCRrzIlP_SoxRHVrma18o9FFg";
const char* chatID = "731794798";
const char* plantIdApiKey = "cHl8vZmJMYY9Dgxb3jRxcBvGAFr3sliBAGS4kiCQe0go1Hr8NJ";  // get from https://plant.id
// ==================================================

// Seeed XIAO ESP32S3 Sense camera pins (don't change these)
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

#define USER_LED 21  // onboard LED on XIAO ESP32S3

WiFiClientSecure secureClient;
UniversalTelegramBot bot(botToken, secureClient);

unsigned long lastBotCheck = 0;

// helper for sending photos
camera_fb_t* currentFrameBuffer = nullptr;
int currentIndex = 0;

bool isMoreDataAvailable() {
  return currentIndex < currentFrameBuffer->len;
}

uint8_t getNextByte() {
  return currentFrameBuffer->buf[currentIndex++];
}

uint8_t* getNextBuffer() {
  return currentFrameBuffer->buf + currentIndex;
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
    config.frame_size   = FRAMESIZE_VGA;  // 640x480 - good quality
    config.jpeg_quality = 8;              // 0-63, lower = better quality
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
    Serial.println("PSRAM found - using VGA resolution");
  } else {
    config.frame_size   = FRAMESIZE_QVGA;  // 320x240 fallback
    config.jpeg_quality = 10;
    config.fb_count     = 1;
    Serial.println("No PSRAM - using QVGA resolution");
  }

  // try to init camera with retry
  esp_err_t err = esp_camera_init(&config);
  
  if (err != ESP_OK) {
    Serial.printf("camera init failed: 0x%x, retrying...\n", err);
    delay(1000);
    
    // try one more time
    err = esp_camera_init(&config);
    
    if (err != ESP_OK) {
      Serial.printf("camera init failed again: 0x%x\n", err);
      Serial.println("Check ribbon cable connection!");
      return;
    }
  }

  Serial.println("camera OK");
  
  // tweak sensor settings for better image quality
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 1);    // -2 to 2, slightly brighter
    s->set_contrast(s, 1);      // -2 to 2
    s->set_saturation(s, 0);    // -2 to 2
    s->set_sharpness(s, 1);     // sharper edges
    s->set_whitebal(s, 1);      // auto white balance on
    s->set_awb_gain(s, 1);      // AWB gain on
    s->set_exposure_ctrl(s, 1); // auto exposure on
    s->set_aec2(s, 1);          // AEC DSP on
    s->set_gain_ctrl(s, 1);     // auto gain on
    s->set_agc_gain(s, 0);      // reset gain
    Serial.println("sensor settings applied");
  }
  
  // discard first few frames - camera needs time to adjust exposure
  Serial.println("warming up camera...");
  for (int i = 0; i < 5; i++) {
    camera_fb_t* warmup = esp_camera_fb_get();
    if (warmup) {
      esp_camera_fb_return(warmup);
    }
    delay(100);
  }
  Serial.println("camera ready");
}

// checks plant health - diseases, pests, nutritional issues
String checkPlantHealth(uint8_t* imgBuf, size_t imgLen) {
  Serial.println("checking plant health...");

  // base64 encode
  size_t encodedLen = ((imgLen + 2) / 3) * 4 + 1;
  unsigned char* encoded = (unsigned char*)malloc(encodedLen);

  if (!encoded) {
    return "memory error";
  }

  size_t outLen = 0;
  mbedtls_base64_encode(encoded, encodedLen, &outLen, imgBuf, imgLen);
  
  String b64image = String((char*)encoded);
  free(encoded);

  // health assessment API endpoint
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
  Serial.printf("health API response: %d\n", code);

  String result = "";

  if (code == 200) {
    String response = http.getString();
    
    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, response);

    if (err) {
      result = "failed to parse response";
    } else {
      bool isHealthy = doc["is_healthy"].as<bool>();
      float healthyProb = doc["is_healthy_probability"].as<float>();

      // check if analysis was successful
      if (healthyProb < 0.1 && !isHealthy) {
        result = "⚠️ Unable to analyze plant health\n\n";
        result += "Possible reasons:\n";
        result += "• Image too blurry - adjust camera focus\n";
        result += "• Plant not clearly visible\n";
        result += "• Poor lighting\n";
        result += "• Camera too far from plant\n\n";
        result += "💡 Try:\n";
        result += "1. Twist camera lens to focus\n";
        result += "2. Get closer to plant (10-30cm)\n";
        result += "3. Use good lighting\n";
        result += "4. Take /photo first to check quality";
      }
      else if (isHealthy) {
        result = "✅ Plant looks healthy!\n";
        result += "Confidence: " + String(healthyProb * 100, 1) + "%\n\n";
        result += "No diseases or pests detected.\n\n";
        result += "💡 Keep monitoring regularly!";
      } else {
        result = "⚠️ Potential issues detected:\n\n";
        
        int numDiseases = doc["diseases"].size();
        if (numDiseases > 0) {
          for (int i = 0; i < min(3, numDiseases); i++) {
            String name = doc["diseases"][i]["name"].as<String>();
            float prob = doc["diseases"][i]["probability"].as<float>();
            
            result += String(i + 1) + ". " + name + "\n";
            result += "   Probability: " + String(prob * 100, 0) + "%\n";
            
            // get treatment if available
            if (doc["diseases"][i]["disease_details"]["treatment"].size() > 0) {
              String treatment = doc["diseases"][i]["disease_details"]["treatment"]["chemical"][0].as<String>();
              if (treatment.length() > 0 && treatment.length() < 100) {
                result += "   Treatment: " + treatment + "\n";
              }
            }
            result += "\n";
          }
        } else {
          result += "No specific diseases identified.\n";
          result += "Healthy probability: " + String(healthyProb * 100, 1) + "%\n\n";
          result += "Plant may be stressed but no clear disease.";
        }
      }
    }
  } else if (code == 401) {
    result = "❌ Invalid API key";
  } else if (code == 429) {
    result = "⚠️ Daily limit reached";
  } else {
    result = "API error: " + String(code);
  }

  http.end();
  return result;
}

// takes a proper photo with warmup frames to fix blur and repeated capture issues
camera_fb_t* capturePhoto() {
  // discard 3 frames to let camera adjust before taking real photo
  for (int i = 0; i < 3; i++) {
    camera_fb_t* discard = esp_camera_fb_get();
    if (discard) {
      esp_camera_fb_return(discard);
    }
    delay(100);
  }
  
  // now take the actual photo
  camera_fb_t* fb = esp_camera_fb_get();
  return fb;
}

// sends image to plant.id API and returns identification result
String identifyPlant(uint8_t* imgBuf, size_t imgLen) {
  Serial.println("encoding image...");

  // base64 encode the image
  size_t encodedLen = ((imgLen + 2) / 3) * 4 + 1;
  unsigned char* encoded = (unsigned char*)malloc(encodedLen);

  if (!encoded) {
    Serial.println("malloc failed");
    return "memory error - try restarting";
  }

  size_t outLen = 0;
  mbedtls_base64_encode(encoded, encodedLen, &outLen, imgBuf, imgLen);
  Serial.printf("image encoded: %d bytes -> %d base64 chars\n", imgLen, outLen);

  // build request body
  String b64image = String((char*)encoded);
  free(encoded);

  String body = "{\"images\":[\"data:image/jpg;base64,";
  body += b64image;
  body += "\"],\"modifiers\":[\"crops_fast\"],\"plant_language\":\"en\",";
  body += "\"plant_details\":[\"common_names\",\"wiki_description\"]}";

  Serial.println("sending to plant.id API...");

  WiFiClientSecure apiClient;
  apiClient.setInsecure();

  HTTPClient http;
  http.begin(apiClient, "https://api.plant.id/v2/identify");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Api-Key", plantIdApiKey);
  http.setTimeout(15000);

  int code = http.POST(body);
  Serial.printf("API response code: %d\n", code);

  String result = "";

  if (code == 200) {
    String response = http.getString();
    Serial.println("API response received");

    // parse JSON response
    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, response);

    if (err) {
      Serial.println("JSON parse failed: " + String(err.c_str()));
      result = "failed to parse response";
    } else {
      int numSuggestions = doc["suggestions"].size();

      if (numSuggestions == 0) {
        result = "no plant found - try clearer photo";
      } else {
        String topName   = doc["suggestions"][0]["plant_name"].as<String>();
        float  topProb   = doc["suggestions"][0]["probability"].as<float>();
        String common    = "";

        // get common name if available
        if (doc["suggestions"][0]["plant_details"]["common_names"].size() > 0) {
          common = doc["suggestions"][0]["plant_details"]["common_names"][0].as<String>();
        }

        result += "🌿 Plant: " + topName + "\n";
        if (common.length() > 0) {
          result += "Common name: " + common + "\n";
        }
        result += "Confidence: " + String(topProb * 100, 1) + "%\n";

        // top 3 matches
        int showCount = min(3, numSuggestions);
        result += "\nTop " + String(showCount) + " matches:\n";
        for (int i = 0; i < showCount; i++) {
          String n = doc["suggestions"][i]["plant_name"].as<String>();
          float  p = doc["suggestions"][i]["probability"].as<float>();
          result += String(i + 1) + ". " + n + " - " + String(p * 100, 0) + "%\n";
        }
      }
    }
  } else if (code == 401) {
    result = "❌ Invalid API key - check plantIdApiKey in code";
    Serial.println("API key error");
  } else if (code == 429) {
    result = "⚠️ Daily limit reached (100/day on free plan)";
    Serial.println("rate limit hit");
  } else {
    result = "API error: " + String(code);
    Serial.println("response: " + http.getString());
  }

  http.end();
  return result;
}

void handleIdentify(String chat_id) {
  bot.sendMessage(chat_id, "📸 Taking photo, hold still...", "");

  // blink LED during capture
  digitalWrite(USER_LED, HIGH);

  camera_fb_t* fb = capturePhoto();  // uses warmup frames for clear image
  digitalWrite(USER_LED, LOW);

  if (!fb) {
    bot.sendMessage(chat_id, "❌ Camera capture failed - try /identify again", "");
    return;
  }

  Serial.printf("photo captured: %d bytes\n", fb->len);
  
  // send photo to telegram
  bot.sendMessage(chat_id, "✅ Photo taken! Sending to you...", "");
  
  currentFrameBuffer = fb;
  currentIndex = 0;
  
  String sent = bot.sendMultipartFormDataToTelegram("sendPhoto", "photo", "img.jpg",
                                                     "image/jpeg", chat_id, fb->len,
                                                     isMoreDataAvailable, getNextByte,
                                                     getNextBuffer, nullptr);
  
  if (sent.indexOf("\"ok\":true") > 0) {
    Serial.println("photo sent to telegram");
    bot.sendMessage(chat_id, "🔍 Now identifying plant...", "");
  } else {
    Serial.println("failed to send photo");
    Serial.println(sent);
  }

  // copy image data to separate buffer before returning frame buffer
  // this frees the camera for future captures during the slow API call
  uint8_t* imgCopy = (uint8_t*)malloc(fb->len);
  size_t imgLen = fb->len;
  
  if (imgCopy) {
    memcpy(imgCopy, fb->buf, fb->len);
  }
  
  esp_camera_fb_return(fb);  // free camera immediately
  fb = nullptr;

  if (!imgCopy) {
    bot.sendMessage(chat_id, "Not enough memory", "");
    return;
  }

  String result = identifyPlant(imgCopy, imgLen);
  free(imgCopy);

  if (result.length() == 0) {
    bot.sendMessage(chat_id, "No result from API", "");
    return;
  }

  String msg = "🌱 Plant Identification Result:\n\n" + result;
  bot.sendMessage(chat_id, msg, "");
  Serial.println("result sent to telegram");
}

void handleHealth(String chat_id) {
  bot.sendMessage(chat_id, "📸 Taking photo for health check...", "");

  digitalWrite(USER_LED, HIGH);
  camera_fb_t* fb = capturePhoto();
  digitalWrite(USER_LED, LOW);

  if (!fb) {
    bot.sendMessage(chat_id, "❌ Camera failed", "");
    return;
  }

  Serial.printf("health check photo: %d bytes\n", fb->len);
  
  // send photo first
  bot.sendMessage(chat_id, "✅ Photo taken! Analyzing health...", "");
  
  currentFrameBuffer = fb;
  currentIndex = 0;
  
  String sent = bot.sendMultipartFormDataToTelegram("sendPhoto", "photo", "img.jpg",
                                                     "image/jpeg", chat_id, fb->len,
                                                     isMoreDataAvailable, getNextByte,
                                                     getNextBuffer, nullptr);
  
  if (sent.indexOf("\"ok\":true") > 0) {
    Serial.println("photo sent");
    bot.sendMessage(chat_id, "🔍 Checking for diseases and pests...", "");
  }

  // copy and analyze
  uint8_t* imgCopy = (uint8_t*)malloc(fb->len);
  size_t imgLen = fb->len;
  
  if (imgCopy) {
    memcpy(imgCopy, fb->buf, fb->len);
  }
  
  esp_camera_fb_return(fb);

  if (!imgCopy) {
    bot.sendMessage(chat_id, "Not enough memory", "");
    return;
  }

  String result = checkPlantHealth(imgCopy, imgLen);
  free(imgCopy);

  String msg = "🌿 Plant Health Assessment:\n\n" + result;
  bot.sendMessage(chat_id, msg, "");
  Serial.println("health result sent");
}

void handlePhoto(String chat_id) {
  bot.sendMessage(chat_id, "📸 Taking photo...", "");

  digitalWrite(USER_LED, HIGH);

  camera_fb_t* fb = capturePhoto();  // uses warmup frames for clear image
  digitalWrite(USER_LED, LOW);

  if (!fb) {
    bot.sendMessage(chat_id, "❌ Camera failed", "");
    return;
  }

  Serial.printf("photo: %d bytes\n", fb->len);
  
  currentFrameBuffer = fb;
  currentIndex = 0;
  
  String sent = bot.sendMultipartFormDataToTelegram("sendPhoto", "photo", "img.jpg",
                                                     "image/jpeg", chat_id, fb->len,
                                                     isMoreDataAvailable, getNextByte,
                                                     getNextBuffer, nullptr);
  
  if (sent.indexOf("\"ok\":true") > 0) {
    Serial.println("photo sent");
  } else {
    bot.sendMessage(chat_id, "Failed to send photo", "");
    Serial.println(sent);
  }

  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);

  pinMode(USER_LED, OUTPUT);
  digitalWrite(USER_LED, LOW);

  // blink to show its alive
  digitalWrite(USER_LED, HIGH);
  delay(200);
  digitalWrite(USER_LED, LOW);

  setupCamera();

  // connect to wifi
  Serial.print("connecting to wifi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nwifi connected");
  Serial.println(WiFi.localIP());

  secureClient.setInsecure();

  // let the other bot (ESP8266) start first
  delay(3000);

  String msg = "📷 Camera module ready!\n\n";
  msg += "/photo - take a photo\n";
  msg += "/identify - identify plant\n";
  msg += "/health - check plant health";
  bot.sendMessage(chatID, msg, "");
  Serial.println("startup message sent");
}

void loop() {
  if (millis() - lastBotCheck > 1500) {
    int msgs = bot.getUpdates(bot.last_message_received + 1);

    for (int i = 0; i < msgs; i++) {
      String chat = bot.messages[i].chat_id;
      String text = bot.messages[i].text;

      Serial.println("got message: " + text);

      if (text == "/identify") {
        handleIdentify(chat);
      }
      else if (text == "/photo") {
        handlePhoto(chat);
      }
      else if (text == "/health") {
        handleHealth(chat);
      }
      // all other commands handled by ESP8266, ignore them here
    }

    lastBotCheck = millis();
  }
}
