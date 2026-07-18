// ESP32-CAM plant identification
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

#include "credentials.h"

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

  // use QVGA for faster upload and lower memory use
  // change to FRAMESIZE_VGA for better accuracy if memory allows
  config.frame_size   = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("camera init failed: 0x%x\n", err);
    return;
  }

  Serial.println("camera OK");
}

// sends image to plant.id API and returns identification result
String identifyPlant(camera_fb_t* fb) {
  Serial.println("encoding image...");

  // base64 encode the image
  size_t encodedLen = ((fb->len + 2) / 3) * 4 + 1;
  unsigned char* encoded = (unsigned char*)malloc(encodedLen);

  if (!encoded) {
    Serial.println("malloc failed");
    return "memory error - try restarting";
  }

  size_t outLen = 0;
  mbedtls_base64_encode(encoded, encodedLen, &outLen, fb->buf, fb->len);
  Serial.printf("image encoded: %d bytes -> %d base64 chars\n", fb->len, outLen);

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

        result += "Plant: " + topName + "\n";
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
    result = "invalid API key - check credentials.h";
    Serial.println("API key error");
  } else if (code == 429) {
    result = "daily limit reached (100/day on free plan)";
    Serial.println("rate limit hit");
  } else {
    result = "API error: " + String(code);
    Serial.println("response: " + http.getString());
  }

  http.end();
  return result;
}

void handleIdentify(String chat_id) {
  bot.sendMessage(chat_id, "Taking photo, hold still...", "");

  // blink LED during capture
  digitalWrite(USER_LED, HIGH);
  delay(200);

  camera_fb_t* fb = esp_camera_fb_get();
  digitalWrite(USER_LED, LOW);

  if (!fb) {
    bot.sendMessage(chat_id, "Camera capture failed - try /identify again", "");
    return;
  }

  Serial.printf("photo captured: %d bytes\n", fb->len);
  bot.sendMessage(chat_id, "Photo taken! Identifying plant...", "");

  String result = identifyPlant(fb);
  esp_camera_fb_return(fb);

  if (result.length() == 0) {
    bot.sendMessage(chat_id, "No result from API", "");
    return;
  }

  String msg = "Plant Identification Result:\n\n" + result;
  bot.sendMessage(chat_id, msg, "");
  Serial.println("result sent to telegram");
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

  bot.sendMessage(chatID, "Camera module ready!\n\nSend /identify to identify your plant", "");
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
      // all other commands handled by ESP8266, ignore them here
    }

    lastBotCheck = millis();
  }
}
