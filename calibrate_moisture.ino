// used this to find calibration values for moisture sensor
// run this, note down raw values for dry and wet, then update main code

#define MOISTURE_PIN A0

void setup() {
  Serial.begin(115200);
  pinMode(MOISTURE_PIN, INPUT);
  delay(1000);
  Serial.println("reading moisture sensor raw values...");
  Serial.println("put sensor in air, then water, note the numbers");
}

void loop() {
  int raw = analogRead(MOISTURE_PIN);
  
  Serial.print("raw: ");
  Serial.println(raw);
  
  delay(500);
}
