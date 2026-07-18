// FS90R Calibration - Find exact neutral to reduce noise
// XIAO ESP32-S3 + FS90R on GPIO 1

#include <ESP32Servo.h>

#define SERVO_PIN 1

Servo myServo;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== FS90R Calibration ===");
  Serial.println("Type values 85-95 to find EXACT neutral (no noise/movement)");
  Serial.println("Once neutral found:");
  Serial.println("  200 = Spin clockwise 2 seconds");
  Serial.println("  201 = Spin anticlockwise 2 seconds");
  
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(90);
  Serial.println("\nStarting at 90. Listen for noise...");
}

void loop() {
  if (Serial.available()) {
    int val = Serial.parseInt();
    while (Serial.available()) Serial.read();
    
    if (val == 0) return;
    
    if (val == 200) {
      Serial.println("-> CLOCKWISE 2s (full speed)");
      myServo.write(180);  // Full speed clockwise
      delay(2000);
      myServo.write(90);
      Serial.println("-> Stopped");
    }
    else if (val == 201) {
      Serial.println("-> ANTICLOCKWISE 2s (full speed)");
      myServo.write(0);  // Full speed anticlockwise
      delay(2000);
      myServo.write(90);
      Serial.println("-> Stopped");
    }
    else if (val >= 0 && val <= 180) {
      Serial.print("-> Setting to: ");
      Serial.println(val);
      myServo.write(val);
    }
  }
}
