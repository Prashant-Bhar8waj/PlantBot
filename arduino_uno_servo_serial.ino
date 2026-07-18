// Arduino Uno Servo Control via Serial Monitor
// Send angle (0-180) through Serial Monitor to control servo

#include <Servo.h>

Servo myServo;

#define SERVO_PIN 9

int currentPos = 90;

void setup() {
  Serial.begin(9600);
  Serial.println("Arduino Uno Servo Serial Control");
  Serial.println("================================");
  
  myServo.attach(SERVO_PIN);
  myServo.write(currentPos);
  
  Serial.println("Servo initialized at 90 degrees");
  Serial.println("\nCommands:");
  Serial.println("- Send angle (0-180) to move servo");
  Serial.println("- Send 'c' or 'center' for 90 degrees");
  Serial.println("- Send 's' or 'status' for current position");
  Serial.println("- Send 'min' for 0 degrees");
  Serial.println("- Send 'max' for 180 degrees");
  Serial.println("\nReady!");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() > 0) {
      handleCommand(input);
    }
  }
}

void handleCommand(String cmd) {
  cmd.toLowerCase();
  
  if (cmd == "c" || cmd == "center") {
    moveServo(90);
    Serial.println("Moved to center (90°)");
  }
  else if (cmd == "s" || cmd == "status") {
    Serial.print("Current position: ");
    Serial.print(currentPos);
    Serial.println("°");
  }
  else if (cmd == "min") {
    moveServo(0);
    Serial.println("Moved to minimum (0°)");
  }
  else if (cmd == "max") {
    moveServo(180);
    Serial.println("Moved to maximum (180°)");
  }
  else {
    // Try to parse as number
    int angle = cmd.toInt();
    
    if (angle >= 0 && angle <= 180) {
      moveServo(angle);
      Serial.print("Moved to ");
      Serial.print(angle);
      Serial.println("°");
    }
    else {
      Serial.println("Invalid command!");
      Serial.println("Send angle (0-180) or 'c'/'s'/'min'/'max'");
    }
  }
}

void moveServo(int pos) {
  pos = constrain(pos, 0, 180);
  myServo.write(pos);
  currentPos = pos;
}
