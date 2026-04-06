// Communication with ESP32 will be over SoftwareSerial (Pins 8, 9)
// Hardware Serial (Pins 0, 1) will be used for Debugging
#include <Arduino.h>
#include <string.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// RX=8, TX=9
SoftwareSerial espSerial(8, 9);

// DHT22 Sensor
#define DHTPIN 4          // DHT22 connected to pin 4
#define DHTTYPE DHT22     // DHT22 (AM2302)
DHT dht(DHTPIN, DHTTYPE);

// DS18B20 Soil Temperature Sensor
#define ONE_WIRE_BUS 3
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Moisture Sensor
const int MOISTURE_PIN = A3;
// Calibration: Measure raw value in air (dry) and in water/wet soil
// Your sensor reads ~1023 in dry air, typical capacitive sensors read ~400-500 in wet soil
const int AIR_VALUE = 1023;   // Calibration value for dry air (raw analog reading)
const int WATER_VALUE = 400;  // Calibration value for wet soil (adjust based on your soil)

// TDS Sensor
#define TDS_PIN A1

// Rain Sensor
#define RAIN_PIN A0

// LDR Sensor (Light)
#define LDR_PIN A2

// PIR Motion Sensor
#define PIR_PIN 5         // PIR sensor connected to digital pin 5

// US-100 Ultrasonic Sensor
#define TRIG_PIN 6
#define ECHO_PIN 7

// Actuator pin
// Actuator pin
const int pumpPin = 10; // Water pump connected to pin 10
bool isPumpOn = false;  // Track pump state

// Flow Sensor YF-S401
#define FLOW_SENSOR_PIN 2
volatile long pulseCount;
unsigned long lastFlowMillis = 0;
float flowRate = 0.0;
unsigned int flowMilliLitres = 0;
unsigned long totalMilliLitres = 0;

void pulseCounter() {
  pulseCount++;
}

void setup() {
  // Start the hardware serial for debugging
  Serial.begin(115200);

  // Start SoftwareSerial for ESP32 communication at 9600 baud (More reliable for SoftSerial)
  espSerial.begin(9600);

  // Initialize DHT22 sensor
  dht.begin();

  // Initialize DS18B20 sensor
  sensors.begin();

  // Initialize PIR sensor pin
  pinMode(PIR_PIN, INPUT);
  
  // Initialize LDR Sensor pin
  pinMode(LDR_PIN, INPUT);

  // Initialize Ultrasonic Sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Debug startup message
  Serial.println("ARDUINO: started, awaiting commands");

  // Set up the pump pin as an output
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, LOW); // Default to off

  // Initialize Flow Sensor
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  pulseCount = 0;
  flowRate = 0.0;
  flowMilliLitres = 0;
  totalMilliLitres = 0;
  lastFlowMillis = millis();
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);
}

// Small, heap-free serial command parser
const unsigned long ALIVE_INTERVAL_MS = 5000;
const unsigned long SENSOR_INTERVAL_MS = 5000; // Read sensor every 5 seconds
static unsigned long lastAliveMillis = 0;
static unsigned long lastSensorMillis = 0;
char cmdBuf[64];
size_t cmdLen = 0;

void handleCommand(const char *cmd) {
  // Trim leading/trailing whitespace
  const char *s = cmd;
  while (*s == ' ' || *s == '\t') s++;
  size_t len = strlen(s);
  while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\r' || s[len-1] == '\n')) len--;

  if (len == 0) return;

  // Copy to a local buffer to operate on
  char t[64];
  size_t copyLen = (len < sizeof(t)-1) ? len : sizeof(t)-1;
  memcpy(t, s, copyLen);
  t[copyLen] = '\0';

  Serial.print("CMD RX: "); Serial.println(t); // Debug to PC

  if (strcmp(t, "PUMP_ON") == 0) {
    digitalWrite(pumpPin, HIGH);
    isPumpOn = true;
    Serial.println("ACTION: pump -> ON");
  } else if (strcmp(t, "PUMP_OFF") == 0) {
    digitalWrite(pumpPin, LOW);
    isPumpOn = false;
    Serial.println("ACTION: pump -> OFF");
    
    // Send final 0 flow packet when pump turns off to clear UI
    espSerial.println("FLOW:0.0,0");
  } else if (strncmp(t, "WIFI_OK:", 8) == 0) {
    Serial.print("✅ ESP32 WiFi: ");
    Serial.println(t + 8);  // Print IP address
  } else if (strcmp(t, "WIFI_FAILED") == 0) {
    Serial.println("❌ ESP32 WiFi FAILED");
  } else if (strcmp(t, "MQTT_OK") == 0) {
    Serial.println("✅ ESP32 MQTT Connected");
  } else if (strcmp(t, "SYSTEM_READY") == 0) {
    Serial.println("✅ ESP32 System Ready!");
  }
}

// Timers for split reporting
const unsigned long ENV_INTERVAL_MS = 60000; // 60 seconds (1 minute) for Environment
const unsigned long SEC_INTERVAL_MS = 5000;  // 5 seconds for Security
static unsigned long lastEnvMillis = 0;
static unsigned long lastSecMillis = 0;

void loop() {
  // ... (Serial command handling remains same) ...
  // Non-blocking read of serial data into cmdBuf
  while (espSerial.available()) {
    int c = espSerial.read();
    if (c < 0) break;
    if (c == '\r') continue; // ignore CR
    if (c == '\n') {
      cmdBuf[cmdLen] = '\0';
      if (cmdLen > 0) handleCommand(cmdBuf);
      cmdLen = 0;
    } else {
      if (cmdLen < sizeof(cmdBuf)-1) {
        cmdBuf[cmdLen++] = (char)c;
      } else {
        // buffer overflow, reset
        cmdLen = 0;
      }
    }
  }

  // ========== IMMEDIATE MOTION DETECTION ==========
  // Check PIR sensor every loop iteration for immediate response
  static int lastMotionState = -1;
  static unsigned long lastMotionTime = 0;
  int currentMotion = digitalRead(PIR_PIN);

  // If state changed
  if (currentMotion != lastMotionState) {
    // Debounce/Rate Limit (10 seconds as requested by user)
    if (millis() - lastMotionTime > 10000) { 
      lastMotionState = currentMotion;
      lastMotionTime = millis();
      
      // Send immediate update to ESP32
      espSerial.print("MOTION:");
      espSerial.println(currentMotion);
      
      // Debug
      Serial.print("ALARM: Motion state changed -> ");
      Serial.println(currentMotion);
    }
  }

  unsigned long now = millis();

  // ========== ENVIRONMENT SENSORS (Every 30s) ==========
  if (now - lastEnvMillis >= ENV_INTERVAL_MS) {
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();
    
    // Read Soil Temperature
    sensors.requestTemperatures(); 
    float soilTemp = sensors.getTempCByIndex(0);
    
    // Read Moisture
    int moistureRaw = analogRead(MOISTURE_PIN);
    int moisturePercent = map(moistureRaw, AIR_VALUE, WATER_VALUE, 0, 100);
    moisturePercent = constrain(moisturePercent, 0, 100);

    // Read TDS
    int tdsRaw = analogRead(TDS_PIN);
    float tdsVoltage = tdsRaw * 5.0 / 1024.0;
    float tdsValue = (133.42 * tdsVoltage * tdsVoltage * tdsVoltage - 255.86 * tdsVoltage * tdsVoltage + 857.39 * tdsVoltage) * 0.5;
    
    // Read Rain Sensor
    int rainRaw = analogRead(RAIN_PIN);
    int rainPercent = map(rainRaw, 1023, 0, 0, 100);
    rainPercent = constrain(rainPercent, 0, 100);

    // Read LDR Sensor (Digital: 1=Dark, 0=Light)
    int lightState = digitalRead(LDR_PIN);

    // Check if readings are valid
    if (!isnan(humidity) && !isnan(temperature)) {
      // Format: ENV:temp,humidity,moist%,rawMoist,soilTemp,tds,rain,light
      espSerial.print("ENV:");
      espSerial.print(temperature, 1);
      espSerial.print(",");
      espSerial.print(humidity, 1);
      espSerial.print(",");
      espSerial.print(moisturePercent);
      espSerial.print(",");
      espSerial.print(moistureRaw);
      espSerial.print(",");
      espSerial.print(soilTemp, 1);
      espSerial.print(",");
      espSerial.print((int)tdsValue);
      espSerial.print(",");
      espSerial.print(rainPercent);
      espSerial.print(",");
      espSerial.println(lightState);
      
      // Debug
      Serial.print("DEBUG TX ENV: ");
      Serial.print(temperature); Serial.print(",");
      Serial.print(humidity); Serial.print(",");
      Serial.print(moisturePercent); Serial.print(", Light=");
      Serial.println(lightState);
    } else {
      espSerial.println("ENV:ERROR");
      Serial.println("DEBUG: Env sensor read error");
    }
    lastEnvMillis = now;
  }

  // ========== SECURITY SENSORS (Every 5s) ==========
  if (now - lastSecMillis >= SEC_INTERVAL_MS) {
     int motionDetected = digitalRead(PIR_PIN);

      // Read Ultrasonic Sensor (Distance in cm)
      digitalWrite(TRIG_PIN, LOW);
      delayMicroseconds(2);
      digitalWrite(TRIG_PIN, HIGH);
      delayMicroseconds(10);
      digitalWrite(TRIG_PIN, LOW);
      
      long duration = pulseIn(ECHO_PIN, HIGH);
      int distanceCm = duration * 0.034 / 2;
      if (distanceCm > 400 || distanceCm < 0) distanceCm = 400;

      // Format: SEC:motion,distance
      espSerial.print("SEC:");
      espSerial.print(motionDetected);
      espSerial.print(",");
      espSerial.println(distanceCm);

      // Debug
      Serial.print("DEBUG TX SEC: Motion=");
      Serial.print(motionDetected);
      Serial.print(", Dist=");
      Serial.println(distanceCm);
      
      lastSecMillis = now;
  }

  // ========== FLOW SENSOR (Every 1s ONLY if PUMP IS ON) ==========
  if (isPumpOn && (now - lastFlowMillis >= 1000)) {
    // Disable interrupts to read pulseCount safely
    detachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN));
    
    // Formula for YF-S401: F = 98 * Q (L/min) ?? No, usually F = 7.5 * Q or similar
    // Common YF-S201: F = 7.5 * Q
    // YF-S401 (White, 0.3-6L/min): F = 98 * Q (Litres/min is Q)
    // So Q = F / 98.
    
    float calibrationFactor = 98.0;
    
    // Pulse count is pulses in (now - lastFlowMillis) milliseconds
    double durationSec = (now - lastFlowMillis) / 1000.0;
    if (durationSec <= 0) durationSec = 1.0; // Prevent div by zero

    // Hz = pulses / seconds
    float flowHz = pulseCount / durationSec;
    
    // Q (L/min) = Hz / 98
    flowRate = flowHz / calibrationFactor;

    // However, if we're getting 0, maybe calibration is wrong or no pulses?
    // Let's print raw pulses too for debugging
    // Serial.print("RawPulses: "); Serial.println(pulseCount);
    
    // Calculate Volume in mL passed since last check
    // Volume = (L/min) * (min) * 1000
    // Volume = (Q) * (durationSec / 60) * 1000
    flowMilliLitres = (flowRate / 60.0) * 1000.0 * durationSec;
    
    // Add to total
    totalMilliLitres += flowMilliLitres;
    
    // Reset Pulse Count
    pulseCount = 0;
    
    // Re-enable interrupts
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);
    lastFlowMillis = now;
    
    // Send to ESP32: FLOW:rate(L/min),volume(mL)
    espSerial.print("FLOW:");
    espSerial.print(flowRate, 2); // 2 decimal places
    espSerial.print(",");
    espSerial.println(totalMilliLitres);
    
    // Debug
    Serial.print("DEBUG FLOW: Pulses=");
    Serial.print(pulseCount);
    Serial.print(", Hz=");
    Serial.print(flowHz);
    Serial.print(", Rate=");
    Serial.print(flowRate, 2);
    Serial.print(" L/min, Vol=");
    Serial.print(totalMilliLitres);
    Serial.println(" mL");
  }
}
