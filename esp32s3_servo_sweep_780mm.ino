// Seeed Studio XIAO ESP32-S3 Servo Control
// 4 Forward + 4 Backward Revolutions
// 140mm stick, 780mm displacement
// Powered from XIAO 5V pin (for small servos only)

#include <ESP32Servo.h>

Servo myServo;

// Servo pin: D0 (GPIO 1) on XIAO ESP32-S3
// You can also use D1-D7 (GPIO 2,3,4,5,6,43,44)
#define SERVO_PIN 1  // D0 on XIAO ESP32-S3

// Movement settings
int currentPos = 90;
int targetPos = 90;
int delayTime = 20;  // Delay between steps (ms)

// Calibration: 140mm stick length
// Target displacement: 780 mm (78 cm)
// Linear displacement: 0° = 0 mm, 180° = 780 mm
// 1° ≈ 4.33 mm of linear travel

void setup() {
  Serial.begin(115200);
  Serial.println("\nXIAO ESP32-S3 Servo Control");
  Serial.println("Board: Seeed Studio XIAO ESP32-S3");
  Serial.println("Servo Pin: D0 (GPIO 1)");
  Serial.println("Stick length: 140mm");
  Serial.println("Movement range:");
  Serial.println("  0 degrees = 0mm");
  Serial.println("  180 degrees = 780mm (78cm)");
  Serial.println();
  
  // Attach servo to D0 (GPIO 1)
  // XIAO ESP32-S3 allows custom PWM frequency
  myServo.attach(SERVO_PIN, 500, 2400);  // pin, min pulse, max pulse
  
  // Start from middle position
  myServo.write(currentPos);
  delay(1000);
  
  Serial.print("Starting position: ");
  Serial.print(currentPos);
  Serial.print(" degrees (");
  Serial.print(degreesToMM(currentPos));
  Serial.println("mm)");
  Serial.println();
  Serial.println("Movement sequence:");
  Serial.println("1. 4 FORWARD revolutions (0-180 deg)");
  Serial.println("2. 4 BACKWARD revolutions (180-0 deg)");
  Serial.println("Total: 8 movements, 1440° rotation");
  Serial.println("Then pause 3 seconds and repeat");
  Serial.println();
  
  delay(2000);
}

void loop() {
  // 4 FORWARD revolutions (0-180 deg)
  Serial.println("\n4 FORWARD REVOLUTIONS");
  
  for (int revolution = 1; revolution <= 4; revolution++) {
    Serial.print("Forward Revolution ");
    Serial.print(revolution);
    Serial.println(" of 4");
    Serial.println("Moving to 180° (780mm)...");
    moveToPosition(180);
    Serial.println("Reached 180°");
    delay(500);
    
    // Return to start for next revolution
    if (revolution < 4) {
      Serial.println("Returning to 0° for next revolution...");
      moveToPosition(0);
      delay(500);
    }
  }
  
  Serial.println("Completed 4 forward revolutions!\n");
  delay(1000);
  
  // 4 BACKWARD revolutions (180-0 deg)
  Serial.println("4 BACKWARD REVOLUTIONS");
  
  for (int revolution = 1; revolution <= 4; revolution++) {
    Serial.print("Backward Revolution ");
    Serial.print(revolution);
    Serial.println(" of 4");
    Serial.println("Moving to 0° (0mm)...");
    moveToPosition(0);
    Serial.println("Reached 0°");
    delay(500);
    
    // Return to start for next revolution
    if (revolution < 4) {
      Serial.println("Returning to 180° for next revolution...");
      moveToPosition(180);
      delay(500);
    }
  }
  
  Serial.println("Completed 4 backward revolutions!\n");
  Serial.println("FULL CYCLE COMPLETE!\n");
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
        Serial.print("Position: ");
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
        Serial.print("Position: ");
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
