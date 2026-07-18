// ESP32-S3 Servo Motor Control
// Simple example to control a servo motor

#include <ESP32Servo.h>

// Create servo object
Servo myServo;

// Servo pin - you can use GPIO 18, 19, 21, or other safe pins
#define SERVO_PIN 18

// Servo position variable
int pos = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-S3 Servo Control");
  
  // Attach servo to pin
  // ESP32 allows for much higher frequency than Arduino
  myServo.attach(SERVO_PIN, 500, 2400);  // pin, min pulse width, max pulse width
  
  Serial.println("Servo attached to pin " + String(SERVO_PIN));
  Serial.println("Ready to control servo");
  
  // Move to center position
  myServo.write(90);
  delay(1000);
}

void loop() {
  // Sweep from 0 to 180 degrees
  Serial.println("Sweeping 0 to 180");
  for (pos = 0; pos <= 180; pos += 1) {
    myServo.write(pos);
    delay(15);
  }
  
  delay(500);
  
  // Sweep back from 180 to 0 degrees
  Serial.println("Sweeping 180 to 0");
  for (pos = 180; pos >= 0; pos -= 1) {
    myServo.write(pos);
    delay(15);
  }
  
  delay(500);
}
