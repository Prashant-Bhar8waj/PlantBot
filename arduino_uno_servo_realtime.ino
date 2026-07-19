// Arduino Uno Servo Real-Time Control
// Send single character or angle for instant response
// No need to press Enter - responds immediately!

#include <Servo.h>

Servo myServo;

#define SERVO_PIN 9

int currentPos = 90;
String inputBuffer = "";

void setup() {
  Serial.begin(9600);
  Serial.println("Real-Time Servo Control");
  Serial.println();
  
  myServo.attach(SERVO_PIN);
  myServo.write(currentPos);
  
  Serial.print("Starting position: ");
  Serial.print(currentPos);
  Serial.println(" degrees");
  Serial.println();
  Serial.println("CONTROLS:");
  Serial.println(" w/W = +10 degrees");
  Serial.println(" s/S = -10 degrees");
  Serial.println(" a/A = +1 degree");
  Serial.println(" d/D = -1 degree");
  Serial.println("  0-9 = Type angle (0-180) + Enter");
  Serial.println(" c = Center (90)");
  Serial.println(" m = Min (0)");
  Serial.println(" x = Max (180)");
  Serial.println();
  Serial.println("Ready! Current: 90°");
}

void loop() {
  if (Serial.available() > 0) {
    char input = Serial.read();
    
    // Single character commands (instant response)
    if (input == 'w' || input == 'W') {
      moveServo(currentPos + 10);
      printPosition();
    }
    else if (input == 's' || input == 'S') {
      moveServo(currentPos - 10);
      printPosition();
    }
    else if (input == 'a' || input == 'A') {
      moveServo(currentPos + 1);
      printPosition();
    }
    else if (input == 'd' || input == 'D') {
      moveServo(currentPos - 1);
      printPosition();
    }
    else if (input == 'c' || input == 'C') {
      moveServo(90);
      Serial.println("Center (90°)");
    }
    else if (input == 'm' || input == 'M') {
      moveServo(0);
      Serial.println("Min (0°)");
    }
    else if (input == 'x' || input == 'X') {
      moveServo(180);
      Serial.println("Max (180°)");
    }
    // Number input for precise angle
    else if (input >= '0' && input <= '9') {
      inputBuffer += input;
      Serial.print(input);  // Echo the digit
    }
    else if (input == '\n' || input == '\r') {
      if (inputBuffer.length() > 0) {
        int angle = inputBuffer.toInt();
        if (angle >= 0 && angle <= 180) {
          moveServo(angle);
          Serial.print(angle);
          Serial.println("°");
        } else {
          Serial.println("Invalid! (0-180 only)");
        }
        inputBuffer = "";
      }
    }
    // Clear buffer on space or invalid input
    else if (input == ' ') {
      inputBuffer = "";
      Serial.println();
      printPosition();
    }
  }
}

void moveServo(int pos) {
  pos = constrain(pos, 0, 180);
  
  if (pos != currentPos) {
    myServo.write(pos);
    currentPos = pos;
  }
}

void printPosition() {
  Serial.print("Position: ");
  Serial.print(currentPos);
  Serial.println("°");
}
