// Arduino Uno Servo Control with Potentiometer
// Turn potentiometer to control servo position

#include <Servo.h>

Servo myServo;

#define SERVO_PIN 9
#define POT_PIN A0

int potValue = 0;
int angle = 0;
int lastAngle = -1;

void setup() {
  Serial.begin(9600);
  Serial.println("Arduino Uno Servo + Potentiometer Control");
  
  myServo.attach(SERVO_PIN);
  pinMode(POT_PIN, INPUT);
  
  Serial.println("Turn potentiometer to control servo");
}

void loop() {
  // Read potentiometer (0-1023)
  potValue = analogRead(POT_PIN);
  
  // Map to servo angle (0-180)
  angle = map(potValue, 0, 1023, 0, 180);
  
  // Only update if angle changed significantly (reduce jitter)
  if (abs(angle - lastAngle) > 2) {
    myServo.write(angle);
    lastAngle = angle;
    
    // Print to Serial Monitor
    Serial.print("Pot: ");
    Serial.print(potValue);
    Serial.print(" -> Angle: ");
    Serial.print(angle);
    Serial.println("°");
  }
  
  delay(15);
}
