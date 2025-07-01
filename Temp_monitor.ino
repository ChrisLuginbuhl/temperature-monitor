// Temperature sensing for boat using Particle Boron and Webhooks
// Notes from Nov 2022:

// Not yet version controled on Github as falcor_iob.

#include <Adafruit_BME680.h>

Adafruit_BME680 bme; // I2C

const unsigned long PUBLISH_PERIOD_MS = 1800000;
const char *FIELD_SEPARATOR = "|||";
const char *EVENT_NAME = "tempSensor";
const char *ALARM_NAME = "tempAlarm";
const float LOW_TEMP_THRESHOLD = 2.0;

bool sensorReady = false;
bool alarmStatus = false;
unsigned long lastPublish = 0;
char buf[256];
int publishCount = 0;

void setup() {
    Serial.begin(9600);

    sensorReady = bme.begin();
}


void loop() {
    if ((publishCount < 10) && (millis() - lastPublish >= (int)PUBLISH_PERIOD_MS/30 && sensorReady) || (millis() - lastPublish >= PUBLISH_PERIOD_MS && sensorReady)) {
        lastPublish = millis();

        float temp = bme.readTemperature(); // degrees C
        float pressure = bme.readPressure() / 100.0; // hPa
        float humidity = bme.readHumidity(); // %

        snprintf(buf, sizeof(buf), "%.02f%s%.02f%s%.01f", temp, FIELD_SEPARATOR, pressure, FIELD_SEPARATOR, humidity);
        Particle.publish(EVENT_NAME, buf, PRIVATE);
        if ((temp < LOW_TEMP_THRESHOLD) && !alarmStatus) {
            Particle.publish(ALARM_NAME, "Boat low temp alarm");
            alarmStatus = true;
        } else if (temp > LOW_TEMP_THRESHOLD + 1.0) {
            alarmStatus = false;
        }
        publishCount++;
    }
}
