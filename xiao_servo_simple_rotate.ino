// Simple FS90R Test - Rotate clockwise then anticlockwise
// XIAO ESP32-S3 + FS90R continuous servo on GPIO 2

#include <ESP32Servo.h>

#define SERVO_PIN 2  // Try GPIO 2 (D1) instead

Servo myServo;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Simple Rotation Test ===");
  
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(90);  // Stop
  delay(2000);
  
  Serial.println("Rotating CLOCKWISE for 3 seconds...");
  myServo.write(180);  // Full speed clockwise
  delay(3000);
  
  Serial.println("STOP");
  myServo.write(90);  // Stop
  delay(1000);
  
  Serial.println("Rotating ANTICLOCKWISE for 3 seconds...");
  myServo.write(0);  // Full speed anticlockwise
  delay(3000);
  
  Serial.println("STOP - Test complete!");
  myServo.write(90);  // Stop
}

void loop() {
  // Nothing - runs once in setup
}
