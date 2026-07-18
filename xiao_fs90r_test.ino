// XIAO ESP32-S3 + FS90R Continuous Rotation Servo Test
// Servo: FS90R (factory continuous rotation)
// Connection:
//   Servo Brown → XIAO GND
//   Servo Red   → XIAO 5V (VBUS)
//   Servo Orange → XIAO D0 (GPIO 1)

#include <ESP32Servo.h>

#define SERVO_PIN 1  // D0 (GPIO 1)

Servo myServo;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== FS90R Servo Test ===");
  Serial.println("Find neutral first:");
  Serial.println("  Type 90, then 88, 89, 91, 92 until servo STOPS");
  Serial.println("");
  Serial.println("Once neutral is found:");
  Serial.println("  200 = FORWARD 1 second");
  Serial.println("  201 = BACKWARD 1 second");
  Serial.println("  202 = STOP NOW");
  
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(90);  // Start at typical neutral
  Serial.println("\nReady. Starting at 90.");
}

void loop() {
  if (Serial.available()) {
    int val = Serial.parseInt();
    while (Serial.available()) Serial.read();  // flush newline
    
    if (val == 0) return;  // skip zeros
    
    if (val == 200) {
      Serial.println("-> FORWARD 1s...");
      myServo.write(100);  // above neutral = forward (adjust after finding neutral)
      delay(1000);
      myServo.write(90);
      Serial.println("-> Stopped. Measure distance!");
    }
    else if (val == 201) {
      Serial.println("-> BACKWARD 1s...");
      myServo.write(80);  // below neutral = backward (adjust after finding neutral)
      delay(1000);
      myServo.write(90);
      Serial.println("-> At home.");
    }
    else if (val == 202) {
      myServo.write(90);
      Serial.println("-> STOPPED at 90");
    }
    else if (val >= 0 && val <= 180) {
      Serial.print("-> Setting servo to: ");
      Serial.println(val);
      myServo.write(val);
    }
  }
}
