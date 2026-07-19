// Arduino Servo - Slow Controlled Movement
// 140mm stick attached to servo - tracks linear displacement
// Smooth, slow movement with adjustable speed

#include <Servo.h>

Servo myServo;

#define SERVO_PIN 9

// Movement settings
int currentPos = 90;        // Start from middle position (adjust if needed)
int targetPos = 90;
int delayTime = 20;         // Delay between steps (ms) - increase for slower movement

// Calibration: 140mm stick length
// Target displacement: 780 mm (78 cm)
// Linear displacement: 0° = 0 mm, 180° = 780 mm
// 1° ≈ 4.33 mm of linear travel

void setup() {
  Serial.begin(9600);
  Serial.println("Servo Slow Movement Control");
  Serial.println("Stick length: 140mm");
  Serial.println("Movement range:");
  Serial.println("  0 degrees = 0mm");
  Serial.println("  180 degrees = 780mm (78cm)");
  Serial.println();
  
  myServo.attach(SERVO_PIN);
  
  // Start from current position (middle)
  myServo.write(currentPos);
  delay(1000);
  
  Serial.print("Starting position: ");
  Serial.print(currentPos);
  Serial.print(" degrees (");
  Serial.print(degreesToMM(currentPos));
  Serial.println("mm)");
  Serial.println();
  Serial.println("Movement sequence:");
  Serial.println("1. 4 FORWARD revolutions (0°  180°)");
  Serial.println("2. 4 BACKWARD revolutions (180°  0°)");
  Serial.println("Total: 8 movements, 1440° rotation");
  Serial.println("Then pause 3 seconds and repeat");
  Serial.println();
  
  delay(2000);
}

void loop() {
  // 4 FORWARD revolutions (0°  180°)
  Serial.println("\n");
  Serial.println("  4 FORWARD REVOLUTIONS ");
  Serial.println("");
  
  for (int revolution = 1; revolution <= 4; revolution++) {
    Serial.print(" Forward Revolution ");
    Serial.print(revolution);
    Serial.println(" of 4");
    Serial.println(" Moving to 180° (780mm)...");
    moveToPosition(180);
    Serial.println(" Reached 180°");
    delay(500);
    
    // Return to start for next revolution
    if (revolution < 4) {
      Serial.println(" Returning to 0° for next revolution...");
      moveToPosition(0);
      delay(500);
    }
  }
  
  Serial.println(" Completed 4 forward revolutions!\n");
  delay(1000);
  
  // 4 BACKWARD revolutions (180°  0°)
  Serial.println("");
  Serial.println("  4 BACKWARD REVOLUTIONS ");
  Serial.println("");
  
  for (int revolution = 1; revolution <= 4; revolution++) {
    Serial.print(" Backward Revolution ");
    Serial.print(revolution);
    Serial.println(" of 4");
    Serial.println(" Moving to 0° (0mm)...");
    moveToPosition(0);
    Serial.println(" Reached 0°");
    delay(500);
    
    // Return to start for next revolution
    if (revolution < 4) {
      Serial.println(" Returning to 180° for next revolution...");
      moveToPosition(180);
      delay(500);
    }
  }
  
  Serial.println(" Completed 4 backward revolutions!\n");
  Serial.println("");
  Serial.println(" FULL CYCLE COMPLETE! ");
  Serial.println("\n");
  Serial.println("Pausing 3 seconds before next cycle...\n");
  delay(3000);
}

// Smooth movement to target position
void moveToPosition(int target) {
  targetPos = constrain(target, 0, 180);
  
  // Move one degree at a time
  if (currentPos < targetPos) {
    // Moving forward
    for (int pos = currentPos; pos <= targetPos; pos++) {
      myServo.write(pos);
      currentPos = pos;
      
      // Print progress every 10 degrees
      if (pos % 10 == 0) {
        Serial.print(" Position: ");
        Serial.print(pos);
        Serial.print("° (");
        Serial.print(degreesToMM(pos));
        Serial.println("mm)");
      }
      
      delay(delayTime);
    }
  } else {
    // Moving backward
    for (int pos = currentPos; pos >= targetPos; pos--) {
      myServo.write(pos);
      currentPos = pos;
      
      // Print progress every 10 degrees
      if (pos % 10 == 0) {
        Serial.print(" Position: ");
        Serial.print(pos);
        Serial.print("° (");
        Serial.print(degreesToMM(pos));
        Serial.println("mm)");
      }
      
      delay(delayTime);
    }
  }
}

// Convert degrees to millimetres of linear travel
// 0° to 180° = 780mm displacement
float degreesToMM(int degrees) {
  return (degrees / 180.0) * 780.0;
}
