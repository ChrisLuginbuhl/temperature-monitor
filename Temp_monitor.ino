// Temperature sensing for Particle Boron with power optimization
#include <Adafruit_BME680.h>

Adafruit_BME680 bme; // I2C
const unsigned long PUBLISH_PERIOD_MS = 600000; // 10 minutes (600,000 ms)
const char *FIELD_SEPARATOR = "|||";
const float LOW_TEMP_THRESHOLD = 2.0;

bool sensorReady = false;
bool alarmStatus = false;
unsigned long lastPublish = 0;
char sensorData[256]; // This will hold our sensor reading
int publishCount = 0;

// Power management variables
const unsigned long SLEEP_DURATION_MS = 570000; // Sleep for 9.5 minutes (570,000 ms)
const unsigned long WAKE_BUFFER_MS = 30000; // Stay awake for 30 seconds for measurements
bool shouldSleep = false;
unsigned long wakeTime = 0;

void setup() {
    Serial.begin(9600);
    
    // Initialize BME680 sensor
    sensorReady = bme.begin();
    
    if (!sensorReady) {
        Serial.println("Could not find a valid BME680 sensor, check wiring!");
        // Still continue - we'll keep trying
    }
    
    // Configure BME680 for lower power consumption
    if (sensorReady) {
        // Set low power gas readings (comment out if not using gas sensor)
        bme.setGasHeater(0, 0); // Turn off gas heater to save power
        
        // Set oversampling to reduce power consumption
        bme.setTemperatureOversampling(BME680_OS_2X);
        bme.setPressureOversampling(BME680_OS_2X);
        bme.setHumidityOversampling(BME680_OS_2X);
        bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    }
    
    // Expose the sensor data as a Particle variable
    Particle.variable("sensorData", sensorData);
    
    // Initialize with default value
    strcpy(sensorData, "0.00|||0.00|||0.0");
    
    // Configure power management
    // Keep cellular connection but reduce power
    Particle.keepAlive(23 * 60); // Keep alive every 23 minutes
    
    // Set publishing mode to reduce cellular overhead
    Particle.setPublishBehavior(PUBLISH_ASYNCHRONOUSLY);
    
    Serial.println("Setup complete. Starting power-optimized temperature monitoring...");
    
    // Take initial reading
    takeReading();
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Check if it's time to take a reading
    // First 10 readings: every minute for testing/verification
    // After that: every 10 minutes with sleep mode
    bool shouldTakeReading = false;
    
    if (publishCount < 10) {
        // First 10 readings: every minute (60,000 ms)
        if (currentMillis - lastPublish >= 60000) {
            shouldTakeReading = true;
        }
    } else {
        // After 10 readings: every 10 minutes
        if (currentMillis - lastPublish >= PUBLISH_PERIOD_MS) {
            shouldTakeReading = true;
        }
    }
    
    if (shouldTakeReading) {
        takeReading();
        
        // Only enter sleep mode after the first 10 readings
        if (publishCount >= 10) {
            wakeTime = currentMillis;
            shouldSleep = true;
            Serial.println("Reading taken. Preparing for sleep...");
        } else {
            Serial.print("Initial reading ");
            Serial.print(publishCount);
            Serial.println("/10 - staying awake");
        }
    }
    
    // Handle sleep logic (only after initial 10 readings)
    if (shouldSleep && (currentMillis - wakeTime >= WAKE_BUFFER_MS)) {
        enterSleepMode();
        shouldSleep = false;
        wakeTime = millis(); // Reset wake time after sleep
    }
    
    // Small delay to prevent excessive CPU usage during wake periods
    delay(100);
}

void takeReading() {
    if (!sensorReady) {
        // Try to reinitialize sensor
        sensorReady = bme.begin();
        if (!sensorReady) {
            Serial.println("Sensor still not ready");
            strcpy(sensorData, "ERROR|||ERROR|||ERROR");
            return;
        }
    }
    
    Serial.println("Taking sensor reading...");
    
    // Force a reading (this will wake up the sensor if it was sleeping)
    if (!bme.performReading()) {
        Serial.println("Failed to perform reading");
        strcpy(sensorData, "ERROR|||ERROR|||ERROR");
        return;
    }
    
    float temp = bme.temperature; // degrees C
    float pressure = bme.pressure / 100.0; // hPa
    float humidity = bme.humidity; // %
    
    // Update the variable (this is what the GitHub workflow will poll)
    snprintf(sensorData, sizeof(sensorData), "%.02f%s%.02f%s%.01f", 
            temp, FIELD_SEPARATOR, pressure, FIELD_SEPARATOR, humidity);
    
    Serial.print("Sensor data: ");
    Serial.println(sensorData);
    
    // Check for low temperature alarm
    if ((temp < LOW_TEMP_THRESHOLD) && !alarmStatus) {
        // Send immediate alarm notification
        Particle.publish("tempAlarm", "Low temperature alarm", PRIVATE);
        alarmStatus = true;
        Serial.println("LOW TEMP ALARM TRIGGERED!");
    } else if (temp > LOW_TEMP_THRESHOLD + 1.0) {
        alarmStatus = false;
    }
    
    lastPublish = millis();
    publishCount++;
    
    Serial.print("Reading #");
    Serial.println(publishCount);
}

void enterSleepMode() {
    Serial.println("Entering sleep mode for power conservation...");
    Serial.flush(); // Ensure all serial data is sent
    
    // Put BME680 into sleep mode to save power
    if (sensorReady) {
        // The BME680 will automatically go to sleep mode when not taking readings
        // No explicit sleep command needed
    }
    
    // Configure wake-up - wake up after sleep duration
    // Using SLEEP_MODE_DEEP for maximum power savings
    // The device will wake up, reconnect to cellular, and continue
    
    SystemSleepConfiguration config;
    config.mode(SystemSleepMode::STOP)
          .duration(SLEEP_DURATION_MS)
          .flag(SystemSleepFlag::WAIT_CLOUD);
    
    // Enter sleep mode
    SystemSleepResult result = System.sleep(config);
    
    // When we wake up, we'll continue from here
    Serial.println("Woke up from sleep mode");
    
    // Small delay to allow system to stabilize
    delay(1000);
}

// Optional: Handle system events for better power management
void handleSystemEvents() {
    // This function can be called periodically to handle system events
    // without blocking the main loop
    Particle.process();
}

// Optional: Function to handle manual wake-up or cloud requests
void cloudWakeUp(const char *event, const char *data) {
    Serial.println("Cloud wake-up requested");
    shouldSleep = false; // Cancel sleep if cloud requests data
    takeReading(); // Take immediate reading
}
