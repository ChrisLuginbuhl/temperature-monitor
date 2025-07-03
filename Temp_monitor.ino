
// Temperature sensing for using Particle Boron and an exposed variable
#include <Adafruit_BME680.h>
Adafruit_BME680 bme; // I2C
const unsigned long PUBLISH_PERIOD_MS = 1800000; // 30 minutes
const char *FIELD_SEPARATOR = "|||";
const float LOW_TEMP_THRESHOLD = 2.0;
bool sensorReady = false;
bool alarmStatus = false;
unsigned long lastPublish = 0;
char sensorData[256]; // This will hold our sensor reading
int publishCount = 0;
void setup() {
    Serial.begin(9600);
    sensorReady = bme.begin();
    // Expose the sensor data as a Particle variable
    Particle.variable("sensorData", sensorData);
    // Initialize with default value
    strcpy(sensorData, "0.00|||0.00|||0.0");
}
void loop() {
    if ((publishCount < 10) && (millis() - lastPublish >= (int)PUBLISH_PERIOD_MS/30 && sensorReady) || 
        (millis() - lastPublish >= PUBLISH_PERIOD_MS && sensorReady)) {
        lastPublish = millis();
        float temp = bme.readTemperature(); // degrees C
        float pressure = bme.readPressure() / 100.0; // hPa
        float humidity = bme.readHumidity(); // %
        // Update the variable (this is what we'll poll)
        snprintf(sensorData, sizeof(sensorData), "%.02f%s%.02f%s%.01f", 
                temp, FIELD_SEPARATOR, pressure, FIELD_SEPARATOR, humidity);
        // Optional: still publish events if you want
        // Particle.publish("tempSensor", sensorData, PRIVATE);
        if ((temp < LOW_TEMP_THRESHOLD) && !alarmStatus) {
            // Particle.publish("tempAlarm", "Boat low temp alarm");
            alarmStatus = true;
        } else if (temp > LOW_TEMP_THRESHOLD + 1.0) {
            alarmStatus = false;
        }
        publishCount++;
    }
}
