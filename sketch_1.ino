// first working version - moisture sensor + OLED
// calibration values found by testing sensor in air and water

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define MOISTURE_PIN A0

// calibration - tested manually
#define DRY_VALUE 95   // in air
#define WET_VALUE 64   // in water

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);  // SDA=4, SCL=5 for ESP8266
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("display init failed");
    while(1);  // stop here
  }
  
  Serial.println("Display OK");
  pinMode(MOISTURE_PIN, INPUT);
}

void loop() {
  int moistureRaw = analogRead(MOISTURE_PIN);
  
  // Convert to percentage (0% = wet, 100% = dry)
  int moisturePercent = map(moistureRaw, WET_VALUE, DRY_VALUE, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);
  
  Serial.print("Raw: ");
  Serial.print(moistureRaw);
  Serial.print(" | Moisture: ");
  Serial.print(moisturePercent);
  Serial.println("%");
  
  // Display on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Soil Moisture:");
  
  display.setTextSize(3);
  display.setCursor(25, 20);
  display.print(moisturePercent);
  display.println("%");
  
  // plant mood based on moisture
  display.setTextSize(2);
  display.setCursor(0, 50);
  
  if(moisturePercent > 70) {
    display.println("Thirsty!");
  } else if(moisturePercent < 30) {
    display.println("Happy :)");
  } else {
    display.println("OK");
  }
  
  display.display();
  delay(500);
}