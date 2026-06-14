// quick test to check if LEDs are wired correctly
// each one blinks in sequence

#define RED_LED 13    // D7
#define YELLOW_LED 15 // D8
#define GREEN_LED 14  // D5

void setup() {
  Serial.begin(115200);
  
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  
  Serial.println("testing LEDs...");
}

void loop() {
  // Test RED LED
  Serial.println("RED LED ON (D7/GPIO13)");
  digitalWrite(RED_LED, HIGH);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  delay(1000);
  
  // Test YELLOW LED
  Serial.println("YELLOW LED ON (D8/GPIO15)");
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);
  delay(1000);
  
  // Test GREEN LED
  Serial.println("GREEN LED ON (D5/GPIO14)");
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);
  delay(1000);
  
  // All OFF
  Serial.println("All LEDs OFF");
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  delay(1000);
}
