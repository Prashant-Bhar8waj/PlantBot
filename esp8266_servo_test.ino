// ESP8266 Continuous Rotation Servo Test
// Board: NodeMCU or Generic ESP8266
// Servo connected to D1 (GPIO 5)

#include <Servo.h>

#define SERVO_PIN      5    // D1 on ESP8266 = GPIO 5
#define SERVO_STOP     86   // EXACT neutral (stops here)
#define SERVO_FWD      90   // clockwise (user calibrated)
#define SERVO_BWD      84   // anticlockwise (user calibrated)
#define TRAVEL_TIME_MS 1000 // 1 second movement

Servo myServo;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP8266 Servo Test ===");
  Serial.println("0-180 = raw value (find neutral)");
  Serial.println("200   = FORWARD 1 second");
  Serial.println("201   = BACKWARD 1 second");
  Serial.println("202   = STOP NOW");

  myServo.attach(SERVO_PIN);
  myServo.write(SERVO_STOP);
  Serial.println("Ready. Servo on D1 (GPIO 5)");
}

void loop() {
  if (Serial.available()) {
    int val = Serial.parseInt();
    while (Serial.available()) Serial.read();  // flush leftover newline

    if (val == 0) return;  // skip 0s from newline parsing

    if (val == 200) {
      Serial.println("-> FORWARD 1s...");
      myServo.write(SERVO_FWD);
      delay(TRAVEL_TIME_MS);
      myServo.write(SERVO_STOP);
      Serial.println("-> Stopped. Measure distance!");
    }
    else if (val == 201) {
      Serial.println("-> BACKWARD 1s...");
      myServo.write(SERVO_BWD);
      delay(TRAVEL_TIME_MS);
      myServo.write(SERVO_STOP);
      Serial.println("-> At home.");
    }
    else if (val == 202) {
      myServo.write(SERVO_STOP);
      Serial.println("-> STOPPED");
    }
    else if (val >= 0 && val <= 180) {
      Serial.print("-> Raw value: ");
      Serial.println(val);
      myServo.write(val);
    }
  }
}
