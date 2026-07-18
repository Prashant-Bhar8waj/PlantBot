// Arduino Uno Servo Control with Buttons
// Use buttons to move servo to preset positions

#include <Servo.h>

Servo myServo;

#define SERVO_PIN 9
#define BUTTON1_PIN 2  // Position 0°
#define BUTTON2_PIN 3  // Position 45°
#define BUTTON3_PIN 4  // Position 90°
#define BUTTON4_PIN 5  // Position 135°
#define BUTTON5_PIN 6  // Position 180°

int currentPos = 90;

void setup() {
  Serial.begin(9600);
  Serial.println("Arduino Uno Servo Button Control");
  
  myServo.attach(SERVO_PIN);
  myServo.write(currentPos);
  
  // Setup buttons with internal pullup resistors
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(BUTTON4_PIN, INPUT_PULLUP);
  pinMode(BUTTON5_PIN, INPUT_PULLUP);
  
  Serial.println("Button assignments:");
  Serial.println("Pin 2 -> 0°");
  Serial.println("Pin 3 -> 45°");
  Serial.println("Pin 4 -> 90°");
  Serial.println("Pin 5 -> 135°");
  Serial.println("Pin 6 -> 180°");
  Serial.println("\nReady!");
}

void loop() {
  // Check each button (LOW = pressed with pullup)
  if (digitalRead(BUTTON1_PIN) == LOW) {
    moveServo(0);
    delay(300);  // Debounce
  }
  
  if (digitalRead(BUTTON2_PIN) == LOW) {
    moveServo(45);
    delay(300);
  }
  
  if (digitalRead(BUTTON3_PIN) == LOW) {
    moveServo(90);
    delay(300);
  }
  
  if (digitalRead(BUTTON4_PIN) == LOW) {
    moveServo(135);
    delay(300);
  }
  
  if (digitalRead(BUTTON5_PIN) == LOW) {
    moveServo(180);
    delay(300);
  }
}

void moveServo(int pos) {
  if (pos != currentPos) {
    myServo.write(pos);
    currentPos = pos;
    
    Serial.print("Servo moved to: ");
    Serial.print(pos);
    Serial.println("°");
  }
}
