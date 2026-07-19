// Seeed Studio XIAO ESP32-S3 Servo Control
// 4 Forward + 4 Backward Revolutions
// NO LIBRARY NEEDED - Uses ESP32 built-in PWM
// 140mm stick, 780mm displacement

// Servo pin: D0 (GPIO 1) on XIAO ESP32-S3
#define SERVO_PIN 1

// PWM settings
#define PWM_FREQ 50        // 50Hz for servo
#define PWM_CHANNEL 0      // PWM channel
#define PWM_RESOLUTION 16  // 16-bit resolution

// Movement settings
int currentPos = 90;
int targetPos = 90;
int delayTime = 20;

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
  
  // Setup PWM for servo
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(SERVO_PIN, PWM_CHANNEL);
  
  // Start from middle position
  writeServo(currentPos);
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

// Write angle to servo using PWM
void writeServo(int angle) {
  angle = constrain(angle, 0, 180);
  
  // Convert angle to PWM duty cycle
  // Servo: 0° = 1ms (500us), 180° = 2ms (2400us)
  // At 50Hz (20ms period), 16-bit resolution
  int minDuty = 1638;  // ~0.5ms pulse
  int maxDuty = 8192;  // ~2.5ms pulse
  
  int duty = map(angle, 0, 180, minDuty, maxDuty);
  ledcWrite(PWM_CHANNEL, duty);
}

// Smooth movement to target position
void moveToPosition(int target) {
  targetPos = constrain(target, 0, 180);
  
  if (currentPos < targetPos) {
    // Moving forward
    for (int pos = currentPos; pos <= targetPos; pos++) {
      writeServo(pos);
      currentPos = pos;
      
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
      writeServo(pos);
      currentPos = pos;
      
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
float degreesToMM(int degrees) {
  return (degrees / 180.0) * 780.0;
}
