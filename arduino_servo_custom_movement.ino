// Arduino Servo - Custom Movement Control
// Control speed and positions via Serial Monitor
// For 140mm stick attached to servo (780mm displacement)

#include <Servo.h>

Servo myServo;

#define SERVO_PIN 9

int currentPos = 90;
int delayTime = 20;  // Speed control (lower = faster, higher = slower)

void setup() {
  Serial.begin(9600);
  Serial.println("Custom Servo Movement");
  Serial.println("Stick: 140mm (Movement: 0° = 0mm, 180° = 780mm)");
  Serial.println();
  
  myServo.attach(SERVO_PIN);
  myServo.write(currentPos);
  
  Serial.print("Current position: ");
  Serial.print(currentPos);
  Serial.print("° (");
  Serial.print(degreesToMM(currentPos));
  Serial.println("mm)");
  Serial.println();
  
  Serial.println("COMMANDS:");
  Serial.println("  0-180     = Move to angle");
  Serial.println("  m<value>  = Move to mm displacement (e.g., m390 = 390mm)");
  Serial.println("  min       = Move to 0mm (0°)");
  Serial.println("  max       = Move to 780mm (180°)");
  Serial.println("  center    = Move to 390mm (90°)");
  Serial.println("  speed <1-100> = Set speed (1=slowest, 100=fastest)");
  Serial.println("  sweep     = Auto sweep 0-780-0mm");
  Serial.println("  status    = Show current position");
  Serial.println();
  Serial.println("Ready!");
  
  delay(1000);
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toLowerCase();
    
    if (input.length() > 0) {
      handleCommand(input);
    }
  }
}

void handleCommand(String cmd) {
  if (cmd == "min") {
    Serial.println("Moving to minimum (0mm / 0°)");
    moveToPosition(0);
  }
  else if (cmd == "max") {
    Serial.println("Moving to maximum (780mm / 180°)");
    moveToPosition(180);
  }
  else if (cmd == "center") {
    Serial.println("Moving to center (390mm / 90°)");
    moveToPosition(90);
  }
  else if (cmd == "sweep") {
    Serial.println("Starting sweep cycle");
    Serial.println("Moving to 780mm...");
    moveToPosition(180);
    delay(1000);
    Serial.println("Moving to 0mm...");
    moveToPosition(0);
    delay(1000);
    Serial.println("Sweep complete");
  }
  else if (cmd == "status") {
    printStatus();
  }
  else if (cmd.startsWith("speed ")) {
    int speed = cmd.substring(6).toInt();
    if (speed >= 1 && speed <= 100) {
      // Convert speed to delay (inverse relationship)
      delayTime = map(speed, 1, 100, 100, 1);
      Serial.print("Speed set to ");
      Serial.print(speed);
      Serial.print(" (delay: ");
      Serial.print(delayTime);
      Serial.println("ms)");
    } else {
      Serial.println("Invalid speed! Use 1-100");
    }
  }
  else if (cmd.startsWith("m")) {
    // Move to millimeter displacement
    int mm = cmd.substring(1).toInt();
    if (mm >= 0 && mm <= 780) {
      int degrees = mmToDegrees(mm);
      Serial.print("Moving to ");
      Serial.print(mm);
      Serial.print("mm (");
      Serial.print(degrees);
      Serial.println("°)");
      moveToPosition(degrees);
    } else {
      Serial.println("Invalid mm! Use 0-780");
    }
  }
  else {
    // Try to parse as angle
    int angle = cmd.toInt();
    if (angle >= 0 && angle <= 180) {
      Serial.print("Moving to ");
      Serial.print(angle);
      Serial.print("° (");
      Serial.print(degreesToMM(angle));
      Serial.println("mm)");
      moveToPosition(angle);
    } else {
      Serial.println("Unknown command! Type 'status' for help");
    }
  }
}

void moveToPosition(int target) {
  target = constrain(target, 0, 180);
  
  if (currentPos < target) {
    for (int pos = currentPos; pos <= target; pos++) {
      myServo.write(pos);
      currentPos = pos;
      delay(delayTime);
    }
  } else {
    for (int pos = currentPos; pos >= target; pos--) {
      myServo.write(pos);
      currentPos = pos;
      delay(delayTime);
    }
  }
  
  printStatus();
}

void printStatus() {
  Serial.print("Position: ");
  Serial.print(currentPos);
  Serial.print("° = ");
  Serial.print(degreesToMM(currentPos), 1);
  Serial.println("mm");
}

// Linear displacement (0° to 180° = 780mm)
float degreesToMM(int degrees) {
  return (degrees / 180.0) * 780.0;
}

int mmToDegrees(int mm) {
  return (mm * 180) / 780;
}
