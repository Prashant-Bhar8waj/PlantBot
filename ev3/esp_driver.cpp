#include <Arduino.h>
#include <SoftwareSerial.h>

#define RX_PIN 44
#define TX_PIN 43

SoftwareSerial swSerial(D5, D6);

void setup() {
  Serial.begin(115200);

  swSerial.begin(9600, SWSERIAL_8N1);
  Serial.println("Software UART initialized");

  pinMode(A0, INPUT);
  pinMode(D2, INPUT_PULLUP);
}

float readPhotoresistor(int numReadings = 5) {
  float_t acc = 0;
  for (size_t i = 0; i < numReadings; i++) {
    acc += analogRead(A0) / 1024.0f;
  }

  return acc / numReadings;
}

void sendCommand(char command, String args = "") {
  swSerial.write(command);
  if (args.length() != 0) {
    swSerial.write(args.length());
    swSerial.print(args);
  }
}

void waitForCommandResult() {
  while (!swSerial.available()) {
    yield();
  }

  swSerial.read();
}

#define MEASURE_POINTS 10
#define MOVE_RANGE 1100

int32_t readCurrentOffset() {
  sendCommand('r');

  while (!swSerial.available()) {
    yield();
  }

  int messageLength = swSerial.read();
  while (swSerial.available() < messageLength) {
    yield();
  }

  String message;
  for (size_t i = 0; i < messageLength; i++) {
    message += (char) swSerial.read();
  }
  
  return message.toInt();
}

void moveBy(int32_t signedDistance) {
  int32_t currentOffset = readCurrentOffset();
  int32_t newOffset = currentOffset + signedDistance;
  newOffset = constrain(newOffset, 0, MOVE_RANGE);

  int32_t actualDistance = newOffset - currentOffset;
  Serial.printf("requested distance: %d, actual distance: %d\n", signedDistance, actualDistance);
  if (actualDistance == 0) {
    return;
  }
  
  sendCommand('s', String(actualDistance));
  waitForCommandResult();
}

void moveToOffset(int32_t targetOffset) {
  int32_t currentOffset = readCurrentOffset();
  Serial.printf("currently at %d, moving to %d\n", currentOffset, targetOffset);
  moveBy(targetOffset - currentOffset);
}

void loop() {
  if (!digitalRead(D2)) {
    delay(1000);
    moveToOffset(0);

    float_t readings[MEASURE_POINTS];
    readings[0] = readPhotoresistor(10);
    Serial.printf("reading 0: %f\n", readings[0]);

    size_t stepSize = MOVE_RANGE / MEASURE_POINTS;
    for (size_t i = 0; i < MEASURE_POINTS - 1; i++) {
      moveBy(stepSize);

      readings[i + 1] = readPhotoresistor(10);
      Serial.printf("reading %d: %f\n", i + 1, readings[i +1]);
    }

    float_t maxReading = 0;
    size_t maxReadingIndex = 0;
    for (size_t i = 0; i < MEASURE_POINTS; i++) {
      if (readings[i] > maxReading) {
        maxReading = readings[i];
        maxReadingIndex = i;
      }
    }

    Serial.printf("max reading index: %d\n", maxReadingIndex);
    moveToOffset(maxReadingIndex * stepSize);
  }
}