// XIAO ESP32-S3 Servo + Light Sensor Test
// Test movement and light reading together

#include <ESP32Servo.h>

#define SERVO_PIN 1   // D0
#define LDR_PIN 2     // D1

// Servo calibration
#define SERVO_STOP    96
#define SERVO_FWD     0
#define SERVO_BWD     180

// Travel settings
#define TOTAL_DISTANCE_CM  30.0
#define TOTAL_TIME_MS      7060
#define SCAN_STEPS         5     // 5 positions for quick test

Servo myServo;
float currentPosition = 0.0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nServo + Light Sensor Test");
  Serial.println("Commands:");
  Serial.println(" s = Scan track and show light at each position");
  Serial.println(" h = Go home (0cm)");
  Serial.println(" e = Go to end (30cm)");
  Serial.println("  0 = Reset position to 0cm");
  
  myServo.attach(SERVO_PIN, 500, 2400);
  myServo.write(SERVO_STOP);
  
  pinMode(LDR_PIN, INPUT);
  
  Serial.println("\nReady!");
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();
    while (Serial.available()) Serial.read();
    
    if (cmd == 's') {
      performScan();
    }
    else if (cmd == 'h') {
      goHome();
    }
    else if (cmd == 'e') {
      goToEnd();
    }
    else if (cmd == '0') {
      currentPosition = 0.0;
      Serial.println("-> Position reset to 0cm");
    }
  }
}

void performScan() {
  Serial.println("\nStarting Scan");
  goHome();
  delay(1000);
  
  float stepSize = TOTAL_DISTANCE_CM / (SCAN_STEPS - 1);
  
  for (int i = 0; i < SCAN_STEPS; i++) {
    float targetPos = i * stepSize;
    
    // Move to position
    float distance = targetPos - currentPosition;
    if (abs(distance) > 0.1) {
      moveDistance(distance);
      delay(500);  // Settle time
    }
    
    // Read light
    int lightValue = analogRead(LDR_PIN);
    
    Serial.print("Position: ");
    Serial.print(currentPosition, 1);
    Serial.print(" cm | Light: ");
    Serial.print(lightValue);
    Serial.print(" | ");
    
    // Bar graph
    int bars = map(lightValue, 1000, 4095, 0, 30);
    for (int j = 0; j < bars; j++) {
      Serial.print("#");
    }
    Serial.println();
  }
  
  Serial.println("\nScan Complete");
  goHome();
}

void goHome() {
  Serial.println("-> Going to START (0cm)");
  moveToPosition(0.0);
}

void goToEnd() {
  Serial.println("-> Going to END (30cm)");
  moveToPosition(TOTAL_DISTANCE_CM);
}

void moveToPosition(float targetCm) {
  float distance = targetCm - currentPosition;
  moveDistance(distance);
}

void moveDistance(float distanceCm) {
  if (abs(distanceCm) < 0.1) return;
  
  float timeMs = (abs(distanceCm) / TOTAL_DISTANCE_CM) * TOTAL_TIME_MS;
  
  Serial.print("Moving ");
  Serial.print(abs(distanceCm), 1);
  Serial.print("cm ");
  Serial.println(distanceCm > 0 ? "FORWARD" : "BACKWARD");
  
  if (distanceCm > 0) {
    myServo.write(SERVO_FWD);
  } else {
    myServo.write(SERVO_BWD);
  }
  
  delay((int)timeMs);
  myServo.write(SERVO_STOP);
  
  currentPosition += distanceCm;
  currentPosition = constrain(currentPosition, 0, TOTAL_DISTANCE_CM);
}
