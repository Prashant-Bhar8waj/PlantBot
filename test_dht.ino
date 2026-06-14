// testing DHT11 sensor
// my sensor was giving weird values so wanted to isolate the issue

#include <DHT.h>

#define DHT_PIN 2      // D4
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);

void setup() {
  Serial.begin(115200);
  pinMode(DHT_PIN, INPUT_PULLUP);
  dht.begin();
  delay(2000);  // sensor needs time to stabilize
  Serial.println("DHT test - reading every 2 seconds");
}

void loop() {
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  Serial.print("Temperature: ");
  if (isnan(temp)) {
    Serial.println("FAILED!");
  } else {
    Serial.print(temp);
    Serial.println(" °C");
  }
  
  Serial.print("Humidity: ");
  if (isnan(humidity)) {
    Serial.println("FAILED!");
  } else {
    Serial.print(humidity);
    Serial.println(" %");
  }
  
  Serial.println("---");
  delay(2000);
}
