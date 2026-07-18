// XIAO ESP32-S3 Light Scanner
// Moves platform along 30cm track and measures light at each position
// Servo on GPIO 1, LDR on GPIO 3 (add later with 10k resistor)

#include <ESP32Servo.h>

#define SERVO_PIN 1
#define LDR_PIN 3        // For future use with resistor

// Servo calibration (from testing)
#define SERVO_STOP    96   // Neutral (stops at 94-98, using middle)
#define SERVO_FWD     0    // Forward (toward end)
#define SERVO_BWD     180  // Backward (toward start)

// Travel calibration
#define TOTAL_DISTANCE_CM  30.0   // Total track length
#define TOTAL_TIME_MS      7060   // Time to travel full 30cm (≈4.25cm/s)
#define SCAN_STEPS         10     // Number of measurement points

Servo myServo;
float currentPosition = 0.0;  // Current position in cm (0 = start, 30 = end)

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Light Scanner ===");
  Serial.println("Commands:");
  Serial.println("  s = Start scan (move forward, measure at each step)");
  Serial.println("  h = Go home (return to start)");
  Serial.println("  e = Go to end");
  Serial.println("  f = Move forward 1cm");
  Serial.println("  b = Move backward 1cm");
  Serial.println("  0 = Reset position to 0 (set current position as home)");
  
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(SERVO_STOP);
  
  Serial.println("\nReady. Platform at START (0cm)");
  Serial.println("Type 's' to start scanning!");
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    while (Serial.available()) Serial.read();  // flush
    
    if (cmd == 's') {
      performScan();
    }
    else if (cmd == 'h') {
      goHome();
    }
    else if (cmd == 'e') {
      goToEnd();
    }
    else if (cmd == 'f') {
      moveDistance(1.0);
    }
    else if (cmd == 'b') {
      moveDistance(-1.0);
    }
    else if (cmd == '0') {
      currentPosition = 0.0;
      Serial.println("-> Position RESET to 0cm (home)");
    }
  }
}

void performScan() {
  Serial.println("\n=== Starting Scan ===");
  goHome();  // Start from beginning
  delay(500);
  
  float stepSize = TOTAL_DISTANCE_CM / (SCAN_STEPS - 1);
  
  for (int i = 0; i < SCAN_STEPS; i++) {
    float targetPos = i * stepSize;
    
    // Move to position
    float distance = targetPos - currentPosition;
    if (abs(distance) > 0.1) {
      moveDistance(distance);
      delay(500);  // Settle time
    }
    
    // Measure light (placeholder)
    int lightValue = readLight();
    
    Serial.print("Position: ");
    Serial.print(currentPosition, 1);
    Serial.print(" cm | Light: ");
    Serial.println(lightValue);
  }
  
  Serial.println("\n=== Scan Complete ===");
  goHome();
}

void goHome() {
  Serial.println("-> Going to START (0cm)...");
  moveToPosition(0.0);
}

void goToEnd() {
  Serial.println("-> Going to END (30cm)...");
  moveToPosition(TOTAL_DISTANCE_CM);
}

void moveToPosition(float targetCm) {
  float distance = targetCm - currentPosition;
  moveDistance(distance);
}

void moveDistance(float distanceCm) {
  if (abs(distanceCm) < 0.1) return;  // Too small, skip
  
  // Calculate time needed
  float timeMs = (abs(distanceCm) / TOTAL_DISTANCE_CM) * TOTAL_TIME_MS;
  
  Serial.print("Moving ");
  Serial.print(abs(distanceCm), 1);
  Serial.print(" cm ");
  Serial.print(distanceCm > 0 ? "FORWARD" : "BACKWARD");
  Serial.print(" (");
  Serial.print((int)timeMs);
  Serial.println(" ms)");
  
  // Move
  if (distanceCm > 0) {
    myServo.write(SERVO_FWD);
  } else {
    myServo.write(SERVO_BWD);
  }
  
  delay((int)timeMs);
  myServo.write(SERVO_STOP);
  
  // Update position
  currentPosition += distanceCm;
  currentPosition = constrain(currentPosition, 0, TOTAL_DISTANCE_CM);
}

int readLight() {
  // Simulated light value (replace with analogRead(LDR_PIN) when LDR is connected)
  return random(100, 4000);
}
