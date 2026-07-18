// XIAO ESP32-S3 Light Sensor Test
// LDR connected to D1 (GPIO 2) with 10k potentiometer

#define LDR_PIN 2  // D1

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(LDR_PIN, INPUT);
  
  Serial.println("\n=== Light Sensor Test ===");
  Serial.println("Cover sensor with hand = LOW values");
  Serial.println("Shine light on sensor = HIGH values");
  Serial.println("\nReading light values...\n");
}

void loop() {
  int lightValue = analogRead(LDR_PIN);
  
  Serial.print("Light: ");
  Serial.print(lightValue);
  Serial.print(" | ");
  
  // Visual bar graph
  int bars = map(lightValue, 0, 4095, 0, 50);
  for (int i = 0; i < bars; i++) {
    Serial.print("█");
  }
  Serial.println();
  
  delay(200);
}
