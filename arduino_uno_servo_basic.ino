// Arduino Uno Servo Motor Control
// Basic sweep example

#include <Servo.h>

Servo myServo;

// Use pin 9 (PWM pin)
#define SERVO_PIN 9

int pos = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Arduino Uno Servo Control");
  
  // Attach servo to pin 9
  myServo.attach(SERVO_PIN);
  
  Serial.println("Servo attached to pin 9");
  
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
