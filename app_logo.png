/*
 * ESP32-CAM Smart Irrigation System with Device-Side Timer Logic
 *
 * ARCHITECTURE:
 * - Timer logic runs FULLY on ESP32-CAM (survives app kill/phone off)
 * - MQTT retained messages store schedule/timer state
 * - NTP for time sync (local clock for execution)
 * - ESP32 sends PUMP_ON/PUMP_OFF commands to Arduino slave via Serial
 * - Arduino controls relay on pin 10
 *
 * PRIORITY: Manual Override > Quick Timer > Schedule
 */

#include "esp_camera.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

// ========== WIFI & MQTT CONFIGURATION ==========
const char *ssid = "Arnabmandal";
const char *password = "hm403496";
const char *mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

// MQTT Topics (using retained messages for persistence)
const char *TOPIC_SCHEDULE = "irrigation_arnab/pump/schedule";
const char *TOPIC_MANUAL = "irrigation_arnab/pump/manual";
const char *TOPIC_STATUS = "irrigation_arnab/pump/status";
const char *TOPIC_SENSORS = "irrigation_arnab/sensors/data";
const char *TOPIC_AUTO = "irrigation_arnab/pump/auto";
const char *TOPIC_SAFETY =
    "irrigation_arnab/pump/safety"; // New topic for safety settings

// NTP Configuration
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800; // Adjust for your timezone (IST = 19800)
const int daylightOffset_sec = 0;

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Preferences prefs;

// ========== GLOBAL OBJECTS ==========
WebServer server(80);

// Camera streaming
volatile uint32_t g_streamOwner = 0;
volatile uint32_t g_streamRequested = 0;
static const bool SERIAL_DEBUG = false;

// Logging helpers - Use Serial (USB) for debug
#define LOG_INFO(fmt, ...)                                                     \
  do {                                                                         \
    Serial.printf((fmt), ##__VA_ARGS__);                                       \
  } while (0)
#define LOG_DBG(fmt, ...)                                                      \
  if (SERIAL_DEBUG)                                                            \
  Serial.printf((fmt), ##__VA_ARGS__)

// ========== PIN DEFINITIONS ==========
#define RELAY_PIN 12 // Active-LOW relay (LOW = ON, HIGH = OFF)

// AI Thinker ESP32-CAM Camera Pins
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// ========== STATE MACHINES & TIMERS ==========
enum PumpMode {
  MODE_OFF,         // Default state
  MODE_MANUAL,      // Manual override active
  MODE_AUTO,        // Auto-irrigation based on sensor
  MODE_QUICK_TIMER, // Quick timer active
  MODE_SCHEDULE     // Scheduled run active
};

struct AutoMode {
  bool enabled;
  int dryThreshold;  // ON threshold (Raw ADC)
  int stopThreshold; // OFF threshold (Raw ADC)
};

struct QuickTimer {
  bool active;
  unsigned long startMillis;
  uint32_t onDelaySec;
  uint32_t offDelayMin;
  bool pumpHasStarted; // Track if ON delay completed
};

struct Schedule {
  bool enabled;
  uint8_t startHour;
  uint8_t startMinute;
  uint32_t durationMin;
  bool isRunning;
  unsigned long startMillis; // When pump actually started
};

struct ManualOverride {
  bool active;
  bool desiredState; // true = ON, false = OFF
};

// State variables
PumpMode currentMode = MODE_OFF;
ManualOverride manualState = {false, false};
QuickTimer quickTimer = {false, 0, 0, 0, false};
Schedule schedule = {false, 0, 0, 0, false, 0};
AutoMode autoMode = {false, 800, 750}; // Default disabled, 800 ON, 750 OFF
bool pumpPhysicalState = false;        // Current relay state (true = ON)
bool ntpSynced = false;
unsigned long lastNtpSync = 0;
unsigned long lastMqttReconnect = 0;
unsigned long lastStatusPublish = 0;
bool quickTimerRestored = false;

// Sensor data from Arduino
float currentTemperature = 0.0;
float currentHumidity = 0.0;
int currentMoisture = 0;
int currentMoistureRaw = 0;
float currentSoilTemperature = 0.0;
int currentTDS = 0;
int currentRain = 0;
int currentMotion = 0; // PIR motion sensor (1 = motion detected, 0 = no motion)
int currentDistance = 0; // Ultrasonic distance in cm
int currentLight = 0;    // LDR sensor (0 = Day, 1 = Night)
bool sensorDataValid = false;
unsigned long lastSensorPublish = 0;

// Water Management Variables
float currentFlowRate = 0.0;
unsigned int currentFlowVol = 0; // Volume in last second (mL)
float dailyVolumeLiters = 0.0;
unsigned long lastFlowRecvTime = 0;
bool dryRunAlertActive = false;

// Safety Settings
bool maxRunEnabled = false;
uint32_t maxRunMinutes = 20; // Default 20 mins
unsigned long pumpStartTime = 0;
int currentDay = -1; // For tracking daily reset

// Dry Run Protection Settings
bool dryRunProtectionEnabled = false;
unsigned long lastFlowDetectedTime = 0;
const unsigned long DRY_RUN_TIMEOUT_MS = 30000; // 30 seconds

// ========== FUNCTION DECLARATIONS ==========
bool initCamera();
void setupWiFi();
void setupNTP();
void syncNTP();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void mqttReconnect();
void processTimerLogic();
void setPumpState(bool state, const char *reason);
void publishStatus();
void handleStream();
void handleCapture();
void handleCaptureJPG();
static uint32_t clientId(const WiFiClient &c);
void restoreQuickTimerFromPrefs();
void clearQuickTimerPrefs();
void restoreScheduleFromPrefs();
void readArduinoSerial();
void publishSensorData();

// ============ SETUP ===========
void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200); // USB Serial Monitor (Debug)
  // Serial1.begin(115200);  // Debug output on GPIO2 (U0TXD alternate) -
  // REMOVED to free pins if needed

  // Initialize Serial2 for Arduino Communication
  // RX = GPIO 14, TX = GPIO 15
  Serial2.begin(9600, SERIAL_8N1, 14, 15);

  LOG_INFO("\n\n========================================\n");
  LOG_INFO("ESP32-CAM Irrigation System Starting\n");
  LOG_INFO("========================================\n");

  // Initialize relay (Active-LOW: HIGH = OFF)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Ensure pump is OFF at boot
  pumpPhysicalState = false;
  Serial2.println("PUMP_OFF"); // Send initial OFF command to Arduino
  delay(100);                  // Give Arduino time to process
  LOG_INFO("Relay initialized (OFF)\n");

  // Initialize preferences
  prefs.begin("irrigation", false);
  restoreScheduleFromPrefs();

  // Init Camera
  if (!initCamera()) {
    LOG_INFO("Camera init failed! Continuing without camera...\n");
  }

  // WiFi Connection
  setupWiFi();

  // Setup NTP time sync
  if (WiFi.status() == WL_CONNECTED) {
    setupNTP();
  }

  // MQTT Setup
  mqttClient.setBufferSize(512); // Increase buffer size for large JSON payloads
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(60);
  mqttClient.setSocketTimeout(10);

  // Web Server
  server.on("/", []() {
    server.sendHeader("Location", "/stream");
    server.send(302, "text/plain", "Redirecting");
  });
  server.on("/stream", handleStream);
  server.on("/capture", handleCapture);
  server.on("/capture.jpg", handleCaptureJPG);

  server.begin();
  LOG_INFO("HTTP server started\n");
  LOG_INFO("System ready!\n");
  Serial2.println("SYSTEM_READY"); // Send status to Serial2
}

// ============ MAIN LOOP ============
void loop() {
  // Handle HTTP requests (non-blocking)
  server.handleClient();

  // Read sensor data from Arduino via Serial2 (Pins 14/15)
  readArduinoSerial();

  // Maintain MQTT connection
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      // Non-blocking reconnect (attempt every 5 seconds)
      if (millis() - lastMqttReconnect > 5000) {
        mqttReconnect();
        lastMqttReconnect = millis();
      }
    } else {
      mqttClient.loop(); // Process MQTT messages
    }

    // Periodic NTP resync (every hour)
    if (ntpSynced && (millis() - lastNtpSync > 3600000)) {
      syncNTP();
    }
  }

  // Restore quick timer once after time sync
  if (ntpSynced && !quickTimerRestored) {
    restoreQuickTimerFromPrefs();
  }

  // *** CRITICAL: Timer logic runs EVERY loop iteration ***
  // This ensures pump control continues even without MQTT messages
  processTimerLogic();

  // Publish status periodically (every 60 seconds)
  if (millis() - lastStatusPublish > 60000) {
    publishStatus();
    lastStatusPublish = millis();
  }

  yield(); // Let ESP32 background tasks run
}

// ========== WIFI SETUP ==========
void setupWiFi() {
  LOG_INFO("Connecting to WiFi: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Disable sleep for reliable operation
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts++ < 40) {
    delay(500);
    Serial1.print("."); // Debug to Serial1, not Serial
  }

  if (WiFi.status() == WL_CONNECTED) {
    LOG_INFO("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    // Send status to Serial2 so Arduino can see it
    Serial2.print("WIFI_OK:");
    Serial2.println(WiFi.localIP().toString());
  } else {
    LOG_INFO("\nWiFi connection failed!\n");
    Serial2.println("WIFI_FAILED");
  }
}

// ========== NTP TIME SYNC ==========
void setupNTP() {
  LOG_INFO("Syncing time with NTP...\n");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Wait for time sync (max 10 seconds)
  int attempts = 0;
  while (!ntpSynced && attempts++ < 20) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      ntpSynced = true;
      lastNtpSync = millis();
      LOG_INFO("NTP synced: %04d-%02d-%02d %02d:%02d:%02d\n",
               timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
               timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      return;
    }
    delay(500);
  }
  LOG_INFO("NTP sync failed!\n");
}

void syncNTP() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    lastNtpSync = millis();
    LOG_INFO("NTP resync: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min,
             timeinfo.tm_sec);
  }
}

// ========== MQTT CALLBACK ==========
// Parses retained messages and updates local state
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  // Convert payload to string
  char message[512];
  if (length >= sizeof(message))
    length = sizeof(message) - 1;
  memcpy(message, payload, length);
  message[length] = '\0';

  LOG_INFO("MQTT RX: %s -> %s\n", topic, message);

  // Parse JSON payload
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    LOG_INFO("JSON parse error: %s\n", error.c_str());
    return;
  }

  String topicStr = String(topic);

  // ========== SCHEDULE TOPIC ==========
  if (topicStr == TOPIC_SCHEDULE) {
    const char *type = doc["type"];

    if (strcmp(type, "SCHEDULE") == 0) {
      // Parse:
      // {"type":"SCHEDULE","start":"18:20","duration_min":60,"enabled":true}
      // OR: {"type":"SCHEDULE","enabled":false} to disable
      schedule.enabled = doc["enabled"];

      if (schedule.enabled) {
        const char *startTime = doc["start"];
        schedule.durationMin = doc["duration_min"];

        // Parse "HH:MM" format
        if (startTime && strlen(startTime) == 5) {
          schedule.startHour = (startTime[0] - '0') * 10 + (startTime[1] - '0');
          schedule.startMinute =
              (startTime[3] - '0') * 10 + (startTime[4] - '0');
        }

        LOG_INFO("Schedule updated: ENABLED at %02d:%02d for %u min\n",
                 schedule.startHour, schedule.startMinute,
                 schedule.durationMin);

        // Store in preferences (survives reboot)
        prefs.putBool("sched_en", true);
        prefs.putUChar("sched_hr", schedule.startHour);
        prefs.putUChar("sched_min", schedule.startMinute);
        prefs.putUInt("sched_dur", schedule.durationMin);
      } else {
        // Schedule disabled - stop if running
        LOG_INFO("Schedule DISABLED\n");
        schedule.isRunning = false;
        prefs.putBool("sched_en", false);
      }
    } else if (strcmp(type, "QUICK_TIMER") == 0) {
      // Parse: {"type":"QUICK_TIMER","on_delay_sec":15,"off_delay_min":1}
      quickTimer.onDelaySec = doc["on_delay_sec"];
      quickTimer.offDelayMin = doc["off_delay_min"];
      quickTimer.active = true;
      quickTimer.startMillis = millis();
      quickTimer.pumpHasStarted = false;

      // Persist quick timer (so it survives reboot / website closed)
      prefs.putBool("qt_active", true);
      prefs.putUInt("qt_on", quickTimer.onDelaySec);
      prefs.putUInt("qt_off", quickTimer.offDelayMin);
      if (ntpSynced) {
        time_t now;
        time(&now);
        if (now > 0)
          prefs.putULong("qt_epoch", (uint32_t)now);
      }

      LOG_INFO("Quick Timer started: ON in %u sec, OFF after %u min\n",
               quickTimer.onDelaySec, quickTimer.offDelayMin);
    }
  }

  // ========== AUTO MODE TOPIC ==========
  else if (topicStr == TOPIC_AUTO) {
    const char *type = doc["type"];

    if (strcmp(type, "AUTO") == 0) {
      // Parse: {"type":"AUTO","enabled":true}
      autoMode.enabled = doc["enabled"];

      // Optional: Update thresholds if provided
      if (doc.containsKey("threshold")) {
        autoMode.dryThreshold = doc["threshold"];
      }
      if (doc.containsKey("stop_threshold")) {
        autoMode.stopThreshold = doc["stop_threshold"];
      }

      LOG_INFO("Auto Mode updated: %s (ON>%d, OFF<%d)\n",
               autoMode.enabled ? "ENABLED" : "DISABLED", autoMode.dryThreshold,
               autoMode.stopThreshold);

      // Persist
      prefs.putBool("auto_en", autoMode.enabled);
      prefs.putInt("auto_thr", autoMode.dryThreshold);
      prefs.putInt("auto_stop_thr", autoMode.stopThreshold);
    }
  }

  // ========== MANUAL OVERRIDE TOPIC ==========
  else if (topicStr == TOPIC_MANUAL) {
    const char *type = doc["type"];

    if (strcmp(type, "MANUAL") == 0) {
      // Parse: {"type":"MANUAL","state":"ON"} or
      // {"type":"MANUAL","state":"OFF"}
      const char *state = doc["state"];

      if (strcmp(state, "OFF") == 0) {
        // User turned pump OFF - apply immediately then release manual control
        // This allows timers/schedules to resume after user intervention
        manualState.active = false; // Release manual mode
        manualState.desiredState = false;
        // Cancel any active quick timer when user explicitly turns OFF
        if (quickTimer.active) {
          quickTimer.active = false;
          quickTimer.pumpHasStarted = false;
          clearQuickTimerPrefs();
          LOG_INFO("Quick Timer cancelled by MANUAL OFF\n");
        }
        LOG_INFO("Manual override: OFF (released)\n");

        // CRITICAL: Clear retained message to prevent re-triggering on
        // reconnect
        mqttClient.publish(TOPIC_MANUAL, "", true); // Empty retained message

        // Apply pump state immediately and publish status
        setPumpState(false, "Manual OFF");
      } else {
        // User turned pump ON - hold manual control
        manualState.active = true;
        manualState.desiredState = true;
        LOG_INFO("Manual override: ON (active)\n");

        // Apply pump state immediately and publish status
        setPumpState(true, "Manual ON");
      }
    }
  }

  // ========== SAFETY SETTINGS TOPIC ==========
  else if (topicStr == TOPIC_SAFETY) {
    const char *type = doc["type"];
    if (strcmp(type, "SAFETY") == 0) {
      // {"type":"SAFETY", "max_run_enabled":true, "max_run_min":20}
      if (doc.containsKey("max_run_enabled"))
        maxRunEnabled = doc["max_run_enabled"];
      if (doc.containsKey("max_run_min"))
        maxRunMinutes = doc["max_run_min"];

      LOG_INFO("Safety updated: MaxRun=%s (%u min)\n",
               maxRunEnabled ? "ON" : "OFF", maxRunMinutes);

      prefs.putBool("safe_en", maxRunEnabled);
      prefs.putUInt("safe_min", maxRunMinutes);

      publishStatus();
    }
    // Handle Dry Run Protection settings
    else if (strcmp(type, "DRY_RUN_PROTECTION") == 0) {
      // {"type":"DRY_RUN_PROTECTION", "enabled":true}
      if (doc.containsKey("enabled")) {
        dryRunProtectionEnabled = doc["enabled"];

        LOG_INFO("Dry Run Protection updated: %s\n",
                 dryRunProtectionEnabled ? "ENABLED" : "DISABLED");

        // Reset timer when enabling
        if (dryRunProtectionEnabled && pumpPhysicalState) {
          lastFlowDetectedTime = millis();
        }

        // Persist setting
        prefs.putBool("dry_run_en", dryRunProtectionEnabled);

        publishStatus();
      }
    }
  }
}

// ========== MQTT RECONNECT (NON-BLOCKING) ==========
void mqttReconnect() {
  LOG_INFO("Attempting MQTT connection...\n");
  Serial.println("MQTT_CONNECTING..."); // Debug: show attempt

  if (mqttClient.connect("ESP32-CAM-Irrigation")) {
    LOG_INFO("MQTT connected!\n");
    Serial2.println("MQTT_OK"); // Send status to Serial2

    // Subscribe to control topics
    mqttClient.subscribe(TOPIC_SCHEDULE, 1); // QoS 1 for reliability
    mqttClient.subscribe(TOPIC_MANUAL, 1);
    mqttClient.subscribe(TOPIC_AUTO, 1);
    mqttClient.subscribe(TOPIC_SAFETY, 1);

    // Restore state from preferences (in case ESP rebooted)
    restoreScheduleFromPrefs();

    // Restore Safety Settings
    maxRunEnabled = prefs.getBool("safe_en", false);
    maxRunMinutes = prefs.getUInt("safe_min", 20);

    // Restore Dry Run Protection Settings
    dryRunProtectionEnabled = prefs.getBool("dry_run_en", false);
    LOG_INFO("Dry Run Protection restored: %s\n",
             dryRunProtectionEnabled ? "ENABLED" : "DISABLED");

    publishStatus();
  } else {
    LOG_INFO("MQTT connect failed, state=%d\n", mqttClient.state());
    Serial2.print("MQTT_FAILED:");
    Serial2.println(mqttClient.state()); // Debug: show error code
  }
}

// ========== TIMER LOGIC (RUNS CONTINUOUSLY) ==========
// This is the HEART of the system - runs independent of MQTT/app
void processTimerLogic() {
  bool desiredPumpState = false;
  PumpMode newMode = MODE_OFF;

  // ========== PRIORITY 1: MANUAL OVERRIDE ==========
  if (manualState.active) {
    desiredPumpState = manualState.desiredState;
    newMode = MODE_MANUAL;
  }
  // ========== PRIORITY 2: AUTO MODE ==========
  else if (autoMode.enabled) {
    // Trigger if Dry (Raw > Threshold)
    // Hysteresis: Turn ON if > dryThreshold, Turn OFF if < stopThreshold
    int onThreshold = autoMode.dryThreshold;
    int offThreshold = autoMode.stopThreshold;

    // Debug Logic - Log every 5 seconds to Serial
    static unsigned long lastDebugTime = 0;
    if (millis() - lastDebugTime > 5000) {
      LOG_INFO("Auto Mode Check: Moisture=%d (On>%d, Off<%d)\n",
               currentMoistureRaw, onThreshold, offThreshold);
      lastDebugTime = millis();
    }

    if (currentMoistureRaw > onThreshold) {
      if (!desiredPumpState)
        LOG_INFO("Auto Mode Trigger: Dry Soil detected (%d > %d) -> Pump ON\n",
                 currentMoistureRaw, onThreshold);
      desiredPumpState = true;
      newMode = MODE_AUTO;
    } else if (currentMoistureRaw < offThreshold) {
      if (desiredPumpState)
        LOG_INFO("Auto Mode Trigger: Wet Soil detected (%d < %d) -> Pump OFF\n",
                 currentMoistureRaw, offThreshold);
      desiredPumpState = false;
      newMode = MODE_AUTO;
    } else {
      // In the hysteresis deadband, keep previous state if it was AUTO
      if (currentMode == MODE_AUTO) {
        desiredPumpState = pumpPhysicalState;
        newMode = MODE_AUTO;
      } else {
        desiredPumpState = false;
        newMode = MODE_AUTO;
      }
    }
  }
  // ========== PRIORITY 3: QUICK TIMER ==========
  else if (quickTimer.active) {
    unsigned long elapsed =
        (millis() - quickTimer.startMillis) / 1000; // seconds

    if (elapsed < quickTimer.onDelaySec) {
      // Still in ON delay period
      desiredPumpState = false;
      newMode = MODE_QUICK_TIMER;
    } else {
      // Pump should be ON; if offDelayMin==0, keep ON until manual OFF
      desiredPumpState = true;
      newMode = MODE_QUICK_TIMER;
      quickTimer.pumpHasStarted = true;

      if (quickTimer.offDelayMin > 0) {
        if (elapsed >= quickTimer.onDelaySec + (quickTimer.offDelayMin * 60)) {
          // Timer expired
          desiredPumpState = false;
          quickTimer.active = false;
          newMode = MODE_OFF;
          clearQuickTimerPrefs();
          LOG_INFO("Quick Timer completed\n");
        }
      }
    }
  }
  // ========== PRIORITY 3: SCHEDULED RUN ==========
  else if (schedule.enabled && ntpSynced) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      uint16_t currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
      uint16_t scheduleMinutes = schedule.startHour * 60 + schedule.startMinute;

      // Check if we're within the scheduled window
      if (!schedule.isRunning) {
        // Check if it's time to start
        if (currentMinutes == scheduleMinutes) {
          schedule.isRunning = true;
          schedule.startMillis = millis();
          LOG_INFO("Schedule started at %02d:%02d\n", timeinfo.tm_hour,
                   timeinfo.tm_min);
        }
      }

      if (schedule.isRunning) {
        unsigned long runningMin = (millis() - schedule.startMillis) / 60000;

        if (runningMin < schedule.durationMin) {
          desiredPumpState = true;
          newMode = MODE_SCHEDULE;
        } else {
          // Schedule completed
          desiredPumpState = false;
          schedule.isRunning = false;
          newMode = MODE_OFF;
          LOG_INFO("Schedule completed\n");
        }
      }
    }
  }

  // Update mode if changed
  if (newMode != currentMode) {
    currentMode = newMode;
    const char *modeNames[] = {"OFF", "MANUAL", "AUTO", "QUICK_TIMER",
                               "SCHEDULE"};
    LOG_INFO("Mode changed: %s\n", modeNames[currentMode]);
  }

  // Apply pump state change if needed
  if (desiredPumpState != pumpPhysicalState) {
    setPumpState(desiredPumpState, "Timer Logic");
  }

  // ========== DRY RUN PROTECTION ==========
  // Monitor flow sensor and stop pump if no flow detected for 30 seconds
  if (dryRunProtectionEnabled && pumpPhysicalState) {
    // Check if we have received flow data recently
    unsigned long timeSincePumpStart = millis() - pumpStartTime;
    unsigned long timeSinceLastFlow = millis() - lastFlowDetectedTime;

    // Only check after pump has been running for 5 seconds (allow time for flow
    // to start)
    if (timeSincePumpStart > 5000) {
      // If no flow detected for 30 seconds, stop the pump
      if (timeSinceLastFlow > DRY_RUN_TIMEOUT_MS) {
        LOG_INFO("DRY RUN DETECTED! No flow for 30s - Stopping pump\n");
        dryRunAlertActive = true;

        // Force pump OFF regardless of mode
        setPumpState(false, "Dry Run Protection");

        // Release all control modes to prevent immediate restart
        manualState.active = false;
        quickTimer.active = false;

        // Clear quick timer from preferences
        clearQuickTimerPrefs();
      }
    }
  }

  // ========== SAFETY LOGIC REMOVED (User Request) ==========
  // Flow data is still collected for display purposes.

  // ========== DAILY RESET ==========
  if (ntpSynced) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      if (currentDay != -1 && currentDay != timeinfo.tm_mday) {
        // New day!
        dailyVolumeLiters = 0.0;
        LOG_INFO("Daily Volume Reset (New Day: %d)\n", timeinfo.tm_mday);
      }
      currentDay = timeinfo.tm_mday;
    }
  }
}

// ========== PUMP CONTROL ==========
// Send commands to Arduino slave via Serial
void setPumpState(bool state, const char *reason) {
  pumpPhysicalState = state;

  if (state) {
    Serial2.println("PUMP_ON");   // Send command to Arduino
    digitalWrite(RELAY_PIN, LOW); // Turn ON (Active-LOW backup)
    LOG_INFO("PUMP ON (%s)\n", reason);

    // Start timers
    if (pumpStartTime == 0) {
      pumpStartTime = millis();
    }

    // Reset dry run protection timer
    lastFlowDetectedTime = millis();
  } else {
    Serial2.println("PUMP_OFF");   // Send command to Arduino
    digitalWrite(RELAY_PIN, HIGH); // Turn OFF (backup)
    digitalWrite(RELAY_PIN, HIGH); // Turn OFF (backup)
    LOG_INFO("PUMP OFF (%s)\n", reason);
    pumpStartTime = 0;        // Reset timer
    lastFlowDetectedTime = 0; // Reset dry run timer
  }

  publishStatus();
}

// ========== STATUS PUBLISHING ==========
void publishStatus() {
  if (!mqttClient.connected())
    return;

  StaticJsonDocument<512> doc;
  doc["pump"] = pumpPhysicalState ? "ON" : "OFF";

  switch (currentMode) {
  case MODE_MANUAL:
    doc["mode"] = "MANUAL";
    break;
  case MODE_AUTO:
    doc["mode"] = "AUTO";
    doc["auto_val"] = currentMoistureRaw;
    doc["auto_target"] = autoMode.dryThreshold;
    doc["auto_stop"] = autoMode.stopThreshold;
    break;
  case MODE_QUICK_TIMER:
    doc["mode"] = "QUICK_TIMER";
    {
      unsigned long elapsed = (millis() - quickTimer.startMillis) / 1000;
      if (quickTimer.pumpHasStarted) {
        unsigned long remaining =
            quickTimer.offDelayMin * 60 - (elapsed - quickTimer.onDelaySec);
        doc["remaining_sec"] = remaining > 0 ? remaining : 0;
      } else {
        doc["on_delay_remaining"] = quickTimer.onDelaySec - elapsed;
      }
    }
    break;
  case MODE_SCHEDULE:
    doc["mode"] = "SCHEDULE";
    if (schedule.isRunning) {
      unsigned long runningMin = (millis() - schedule.startMillis) / 60000;
      unsigned long remainingMin = schedule.durationMin - runningMin;
      doc["remaining_min"] = remainingMin > 0 ? remainingMin : 0;
    }
    break;
  default:
    doc["mode"] = "OFF";
  }

  if (ntpSynced) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeBuf[32];
      sprintf(timeBuf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min,
              timeinfo.tm_sec);
      doc["time"] = timeBuf;
    }
  }

  // Expose schedule / quick-timer / NTP state so UI can show device-side
  // schedule info
  doc["schedule_enabled"] = schedule.enabled;
  char schedStart[6];
  sprintf(schedStart, "%02d:%02d", schedule.startHour, schedule.startMinute);
  doc["schedule_start"] = schedStart;
  doc["schedule_duration_min"] = schedule.durationMin;
  doc["ntp_synced"] = ntpSynced;
  doc["quick_active"] = quickTimer.active;
  if (quickTimer.active) {
    doc["quick_on_delay_sec"] = quickTimer.onDelaySec;
    doc["quick_off_min"] = quickTimer.offDelayMin;
  }

  doc["auto_enabled"] = autoMode.enabled;
  doc["auto_threshold"] = autoMode.dryThreshold;

  // Water data and dry run protection status
  doc["flow_rate"] = currentFlowRate;
  doc["today_vol"] = dailyVolumeLiters;
  doc["dry_run_alert"] = dryRunAlertActive;
  doc["dry_run_protection"] = dryRunProtectionEnabled;

  char buffer[512];
  serializeJson(doc, buffer);
  // Publish retained status so clients receive latest device state on subscribe
  mqttClient.publish(TOPIC_STATUS, buffer, true);
}

// ========== SCHEDULE PERSISTENCE ==========
void restoreScheduleFromPrefs() {
  schedule.enabled = prefs.getBool("sched_en", false);
  schedule.startHour = prefs.getUChar("sched_hr", 0);
  schedule.startMinute = prefs.getUChar("sched_min", 0);
  schedule.durationMin = prefs.getUInt("sched_dur", 0);
  schedule.isRunning = false;
  schedule.startMillis = 0;

  if (schedule.enabled) {
    LOG_INFO("Restored schedule: %02d:%02d for %u min\n", schedule.startHour,
             schedule.startMinute, schedule.durationMin);
  }

  // Restore Auto Mode
  autoMode.enabled = prefs.getBool("auto_en", false);
  autoMode.dryThreshold = prefs.getInt("auto_thr", 800);
  autoMode.stopThreshold = prefs.getInt("auto_stop_thr", 750);
  LOG_INFO("Restored Auto Mode: %s (ON>%d, OFF<%d)\n",
           autoMode.enabled ? "ENABLED" : "DISABLED", autoMode.dryThreshold,
           autoMode.stopThreshold);
}

// ========== QUICK TIMER PERSISTENCE ==========
void clearQuickTimerPrefs() {
  prefs.putBool("qt_active", false);
  prefs.putUInt("qt_on", 0);
  prefs.putUInt("qt_off", 0);
  prefs.putULong("qt_epoch", 0);
}

void restoreQuickTimerFromPrefs() {
  if (quickTimerRestored)
    return;
  if (!prefs.getBool("qt_active", false)) {
    quickTimerRestored = true;
    return;
  }

  uint32_t onDelay = prefs.getUInt("qt_on", 0);
  uint32_t offDelay = prefs.getUInt("qt_off", 0);
  uint32_t startEpoch = prefs.getULong("qt_epoch", 0);

  time_t now;
  time(&now);
  if (startEpoch == 0 || now <= 0 || now < (time_t)startEpoch) {
    LOG_INFO("Quick Timer restore skipped (invalid time)\n");
    quickTimerRestored = true;
    return;
  }

  uint32_t elapsed = (uint32_t)(now - startEpoch);
  uint32_t total = onDelay + (offDelay * 60);

  if (offDelay > 0 && elapsed >= total) {
    // Timer already completed
    clearQuickTimerPrefs();
    quickTimerRestored = true;
    LOG_INFO("Quick Timer restore: expired\n");
    return;
  }

  quickTimer.onDelaySec = onDelay;
  quickTimer.offDelayMin = offDelay;
  quickTimer.active = true;
  quickTimer.startMillis = millis() - (elapsed * 1000UL);
  quickTimer.pumpHasStarted = (elapsed >= onDelay);
  quickTimerRestored = true;

  LOG_INFO(
      "Quick Timer restored: ON in %u sec, OFF after %u min (elapsed %u sec)\n",
      quickTimer.onDelaySec, quickTimer.offDelayMin, elapsed);
}

// ========== READ SENSOR DATA FROM ARDUINO ==========
// ========== READ SENSOR DATA FROM ARDUINO ==========
void readArduinoSerial() {
  static char buffer[128];
  static uint8_t bufferIndex = 0;

  while (Serial2.available()) {
    char c = Serial2.read();

    // ECHO to USB Serial for debugging
    Serial.print(c);

    // Skip carriage return
    if (c == '\r')
      continue;

    if (c == '\n') {
      buffer[bufferIndex] = '\0';

      // === ENVIRONMENT DATA (Every 30s) ===
      // Format: ENV:temp,hum,moist%,raw,soilT,tds,rain
      if (strncmp(buffer, "ENV:", 4) == 0) {
        if (strcmp(buffer + 4, "ERROR") == 0) {
          LOG_INFO("\nEnv sensor read error\n");
        } else {
          char *token = strtok(buffer + 4, ",");
          if (token)
            currentTemperature = atof(token);

          token = strtok(NULL, ",");
          if (token)
            currentHumidity = atof(token);

          token = strtok(NULL, ",");
          if (token)
            currentMoisture = atoi(token);

          token = strtok(NULL, ",");
          if (token)
            currentMoistureRaw = atoi(token);

          token = strtok(NULL, ",");
          if (token)
            currentSoilTemperature = atof(token);

          token = strtok(NULL, ",");
          if (token)
            currentTDS = atoi(token);

          token = strtok(NULL, ",");
          if (token)
            currentRain = atoi(token);

          token = strtok(NULL, ",");
          if (token)
            currentLight = atoi(token);

          // We received valid ENV data, mark as valid so we can publish
          sensorDataValid = true;

          LOG_INFO("\n[ENV] T=%.1f H=%.1f M=%d%% S=%.1f Tds=%d R=%d%% L=%d\n",
                   currentTemperature, currentHumidity, currentMoisture,
                   currentSoilTemperature, currentTDS, currentRain,
                   currentLight);

          publishEnvData(); // Publish immediately on receipt
        }
      }

      // === SECURITY DATA (Every 5s) ===
      // Format: SEC:motion,distance
      else if (strncmp(buffer, "SEC:", 4) == 0) {
        char *token = strtok(buffer + 4, ",");
        if (token)
          currentMotion = atoi(token);

        token = strtok(NULL, ",");
        if (token)
          currentDistance = atoi(token);

        // Security data also validates system is working
        sensorDataValid = true;

        LOG_INFO("\n[SEC] Mot=%d Dist=%dcm\n", currentMotion, currentDistance);

        publishSecData(); // Publish immediately on receipt
      }

      // === IMMEDIATE MOTION EVENT (Interrupt) ===
      // Format: MOTION:1 or MOTION:0
      else if (strncmp(buffer, "MOTION:", 7) == 0) {
        int motionState = atoi(buffer + 7);
        currentMotion = motionState;
        LOG_INFO("\n🔔 IMMEDIATE MOTION EVENT: %d\n", currentMotion);

        // Immediately publish for ultra-low latency
        publishSecData();
      }

      // === FLOW DATA (Every 1s when Pump ON) ===
      // Format: FLOW:rate,vol
      else if (strncmp(buffer, "FLOW:", 5) == 0) {
        char *token = strtok(buffer + 5, ",");
        if (token)
          currentFlowRate = atof(token);

        token = strtok(NULL, ",");
        if (token) {
          if (currentFlowRate > 0) {
            float litersInSec = currentFlowRate / 60.0;
            dailyVolumeLiters += litersInSec;
          }
        }

        lastFlowRecvTime = millis();

        // Update dry run protection timer if flow is detected
        if (currentFlowRate > 0.1) {
          lastFlowDetectedTime = millis();
          dryRunAlertActive = false; // Clear alert if flow resumes
        }

        LOG_INFO("FLOW: %.2f L/min, Daily=%.2f L\n", currentFlowRate,
                 dailyVolumeLiters);

        // Publish immediately when flow data is received
        publishStatus();
      }

      // Reset buffer
      bufferIndex = 0;
    } else {
      if (bufferIndex < sizeof(buffer) - 1) {
        buffer[bufferIndex++] = c;
      } else {
        // Buffer overflow, reset
        bufferIndex = 0;
      }
    }
  }
}

// ========== PUBLISH SENSOR DATA TO MQTT ==========

// ========== PUBLISH SENSOR DATA TO MQTT ==========

// ============ CAMERA INIT ============
bool initCamera() {
  LOG_INFO("Initializing camera...\n");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    LOG_INFO("PSRAM found\n");
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    LOG_INFO("No PSRAM\n");
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    LOG_INFO("Camera init failed: 0x%x\n", err);
    return false;
  }

  LOG_INFO("Camera initialized\n");
  return true;
}

// ========== HTTP HANDLERS ==========
static uint32_t clientId(const WiFiClient &c) {
  IPAddress ip = c.remoteIP();
  return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) |
         ((uint32_t)ip[2] << 8) | (uint32_t)ip[3] ^ (uint32_t)c.remotePort();
}

// ============ STREAM HANDLER ============
void handleStream() {
  WiFiClient client = server.client();
  if (!client)
    return;
  client.setNoDelay(true);

  uint32_t myId = clientId(client);
  LOG_DBG("Stream request from id=%u\n", myId);

  g_streamRequested = myId;
  int wait = 0;
  while (g_streamOwner != 0 && g_streamOwner != myId && wait++ < 50) {
    delay(20);
  }

  if (g_streamOwner != 0 && g_streamOwner != myId) {
    LOG_INFO("Stream busy\n");
    g_streamRequested = 0;
    server.send(503, "text/plain", "Stream busy");
    return;
  }

  g_streamOwner = myId;
  g_streamRequested = 0;
  LOG_INFO("Stream started for id=%u\n", myId);

  const char HEADER[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Cache-Control: no-cache\r\n"
      "Connection: keep-alive\r\n\r\n";
  client.write(HEADER, sizeof(HEADER) - 1);

  while (client.connected()) {
    // Keep MQTT alive during streaming
    if (mqttClient.connected()) {
      mqttClient.loop();
    }

    // *** CRITICAL: Continue timer logic during streaming ***
    processTimerLogic();

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      LOG_DBG("Frame capture failed\n");
      delay(100);
      continue;
    }

    LOG_DBG("Streaming frame len=%u\n", (unsigned)fb->len);

    if (fb->len > 0) {
      char lenBuf[64];
      int headerLen = snprintf(
          lenBuf, sizeof(lenBuf),
          "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n",
          fb->len);
      int wroteHdr = client.write(lenBuf, headerLen);

      // Write image in chunks
      size_t wroteImg = 0;
      const uint8_t *data = (const uint8_t *)fb->buf;
      size_t remaining = fb->len;
      const size_t CHUNK = 1024;
      while (remaining && client.connected()) {
        size_t toWrite = (remaining > CHUNK) ? CHUNK : remaining;
        int r = client.write(data + wroteImg, toWrite);
        if (r > 0) {
          wroteImg += r;
          remaining -= r;
        } else
          break;

        // Keep MQTT processing during large writes
        if (mqttClient.connected())
          mqttClient.loop();
      }

      int wroteTail = client.write("\r\n", 2);

      if (wroteHdr != headerLen || wroteImg != fb->len || wroteTail != 2) {
        LOG_INFO("Client write failed, ending stream\n");
        esp_camera_fb_return(fb);
        break;
      }
    }

    // Check for takeover request
    if (g_streamRequested != 0 && g_streamRequested != g_streamOwner) {
      LOG_INFO("Stream takeover requested\n");
      esp_camera_fb_return(fb);
      break;
    }

    esp_camera_fb_return(fb);
    yield();
  }

  if (g_streamOwner == myId)
    g_streamOwner = 0;
  g_streamRequested = 0;
  LOG_INFO("Stream ended for id=%u\n", myId);
}

// ============ CAPTURE HANDLERS ============
void handleCapture() {
  LOG_DBG("HTTP /capture requested\n");
  server.sendHeader("Access-Control-Allow-Origin", "*");

  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) {
    server.send(200, "text/plain", "OK");
    esp_camera_fb_return(fb);
  } else {
    server.send(500, "text/plain", "Failed to capture frame");
  }
}

void handleCaptureJPG() {
  LOG_DBG("HTTP /capture.jpg requested\n");

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Failed to capture frame");
    return;
  }

  WiFiClient client = server.client();
  if (!client) {
    esp_camera_fb_return(fb);
    return;
  }

  client.setNoDelay(true);
  char hdr[128];
  int hdrLen = snprintf(hdr, sizeof(hdr),
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: image/jpeg\r\n"
                        "Content-Length: %u\r\n"
                        "Access-Control-Allow-Origin: *\r\n\r\n",
                        (unsigned)fb->len);

  if (hdrLen > 0) {
    client.write(hdr, hdrLen);
  }

  // Send image in chunks
  size_t written = 0;
  const uint8_t *cbuf = (const uint8_t *)fb->buf;
  size_t remaining = fb->len;
  const size_t CHUNK = 1024;
  while (remaining && client.connected()) {
    size_t toWrite = (remaining > CHUNK) ? CHUNK : remaining;
    int r = client.write(cbuf + written, toWrite);
    if (r > 0) {
      written += r;
      remaining -= r;
    } else
      break;
  }

  LOG_DBG("Sent %u/%u bytes\n", (unsigned)written, (unsigned)fb->len);
  esp_camera_fb_return(fb);
}
void publishEnvData() {
  if (!mqttClient.connected())
    return;

  StaticJsonDocument<512> doc;
  doc["type"] = "SENSOR_DATA";
  doc["temperature"] = currentTemperature;
  doc["humidity"] = currentHumidity;
  doc["moisture"] = currentMoisture;
  doc["moisture_raw"] = currentMoistureRaw;
  doc["soilTemperature"] = currentSoilTemperature;
  doc["tds"] = currentTDS;
  doc["rain"] = currentRain;
  doc["light"] = currentLight;
  doc["flow"] = currentFlowRate;        // Add flow rate (L/min)
  doc["today_vol"] = dailyVolumeLiters; // Add daily water usage (L)

  // Add timestamp if NTP is synced
  if (ntpSynced) {
    time_t now;
    time(&now);
    doc["timestamp"] = now;
  }

  char buffer[512];
  serializeJson(doc, buffer);
  mqttClient.publish(TOPIC_SENSORS, buffer);

  LOG_INFO("Published ENV data\n");
}

void publishSecData() {
  if (!mqttClient.connected())
    return;

  StaticJsonDocument<256> doc;
  doc["type"] = "SENSOR_DATA"; // Keep type same for UI compatibility
  doc["motion"] = currentMotion;
  doc["distance"] = currentDistance;

  // Add timestamp if NTP is synced
  if (ntpSynced) {
    time_t now;
    time(&now);
    doc["timestamp"] = now;
  }

  char buffer[256];
  serializeJson(doc, buffer);
  mqttClient.publish(TOPIC_SENSORS, buffer);

  LOG_INFO("Published SEC data: Mot=%d Dist=%d\n", currentMotion,
           currentDistance);
}
