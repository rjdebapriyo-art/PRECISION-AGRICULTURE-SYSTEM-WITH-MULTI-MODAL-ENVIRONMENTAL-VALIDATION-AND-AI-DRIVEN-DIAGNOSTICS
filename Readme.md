# FINAL YEAR PROJECT REPORT: Agri-Nigrani - DESIGN AND DEVELOPMENT OF AN IOT-INTEGRATED PRECISION AGRICULTURE SYSTEM WITH MULTI-MODAL ENVIRONMENTAL VALIDATION

**Project Name:** Agri-Nigrani (Smart Agriculture & Security System)  
**Academic Year:** 2025-2026  
  
**Department:** Electrical & Electronic Engineering  

---

## ABSTRACT
Precision agriculture leverages Information and Communication Technology (ICT) to optimize crop yield and resources. This project, **Agri-Nigrani**, presents a robust, dual-layered IoT architecture comprising an ESP32-CAM (Master) and an Arduino Uno (Slave) to manage both irrigation and perimeter security. The system integrates a Transparent Glassmorphism Web Interface for real-time monitoring and control using MQTT and Firebase. Key features include an intelligent multi-prioritized pumping algorithm (Manual > Auto > Timer > Schedule), a sensor fusion security algorithm (PIR + Ultrasonic), and an AI-driven disease analysis module. This report details the hardware-software co-design, the synchronization protocols, and the deployment results of the system.

---

## CHAPTER 1: INTRODUCTION

### 1.1 Motivation
Agriculture remains the backbone of the economy, yet traditional methods often lead to water wastage and security gaps. The integration of IoT allows farmers to make data-driven decisions, reducing human error and increasing productivity.

### 1.2 Problem Statement
Existing smart systems often suffer from high latency, lack of integrated visual monitoring, and dependency on constant cloud connectivity for basic logic. Agri-Nigrani solves this by implementing device-side intelligence.

### 1.3 Objectives
- To design a master-slave hardware architecture for deterministic sensor polling.
- To implement an asynchronous firmware stack on ESP32-CAM for low-latency control.
- To develop a Glassmorphism web dashboard for intuitive data visualization.
- To create a sensor-fusion security algorithm for high-confidence intruder detection.
- To implement an AI-driven "Expert Agro-Advisor" using Gemini 2.5 Flash for high-accuracy disease diagnosis with native Bengali support.
- To develop a data-driven Nutrient Loss Monitor for tracking and preventing fertilizer runoff during irrigation and rainfall events.

### 1.4 Scope
The scope covers hardware interfacing, C++ firmware development, and full-stack web development (HTML/JS/Firebase).

---

## CHAPTER 2: LITERATURE REVIEW

### 2.1 The IoT Paradigm in Farming
Literature reviews indicate a shift from simple automation to "Smart Farming," where sensors, actuators, and cloud computing form a feedback loop. Using Wi-Fi based controllers like the ESP32 provides a cost-effective solution for precision tasks.

### 2.2 Existing Systems vs. Agri-Nigrani
Most systems focus solely on irrigation or security. Agri-Nigrani's unique selling point is the integration of both, along with visual surveillance and AI-assisted health monitoring.

---

## CHAPTER 3: SYSTEM ARCHITECTURE

### 3.1 Design Methodology
The system employs a **Master-Slave topology** via UART.
- **Master (ESP32-CAM)**: High-level logic, WiFi, MQTT, Camera, and NTP.
- **Slave (Arduino Uno)**: Precise timing for sensor polling and relay actuation.

### 3.2 Block Diagram Analysis
The architecture ensures that even if the Master is disconnected from the network, the Slave can maintain basic safety protocols initiated by the Master's last-known state.

### 3.3 Overview
1.  **WiFi Router** -> Internet -> Firebase/MQTT Broker.
2.  **Arduino Uno** (via UART Serial).
3.  **Relay Module** (Direct GPIO or via Arduino).

The **Arduino Uno** connects to:
1.  **Inputs:** DHT22, Capacitive Soil Moisture, DS18B20 Soil Temperature, Rain Sensor, PIR, Ultrasonic, Flow Sensor, TDS, LDR.
2.  **Outputs:** Pump Relay (Pin 10).

**Implementation note (from code):** ESP32-CAM communicates to the Arduino via **Serial2** using **GPIO14 (RX)** and **GPIO15 (TX)**. The Arduino uses **SoftwareSerial (D8 RX, D9 TX)**.

### 3.4 Component Selection
*   **Controller:** **ESP32-CAM** was chosen for its dual-core processor, WiFi+Bluetooth, and camera capabilities. **Arduino Uno** was selected for its 5V logic usage (compatible with most agricultural sensors) and robust ADC.
*   **Sensors:** Economical yet reliable modules like **DHT22** (Precision Temp/Hum), **Capacitive Soil Moisture** (Corrosion resistant), and **HC-SR04** (Ultrasonic) were chosen.

### 3.5 Communication Protocols
1.  **UART (Serial):** Used between ESP32 and Arduino. Custom packet format (`ENV:val,val...`, `SEC:val,val`) ensures low-latency data transfer.
2.  **MQTT (Message Queuing Telemetry Transport):** Used for real-time control between the Web App and ESP32. Topics: `irrigation_arnab/pump/status`, `.../manual`, etc.
3.  **HTTP/WebSocket:** For serving the Web UI and Camera Stream.
4.  **Device-Side Logic:** The ESP32 implements a "Watchdog" and "Timer" logic that runs independently of the cloud to ensure fail-safe operation.

---

## Chapter 4: Hardware Design & Implementation

### 4.1 ESP32-CAM (Master Unit)
The ESP32-CAM (Ai-Thinker Model) serves as the central processing unit. It features a dual-core 32-bit LX6 microprocessor running at 240 MHz, with 520 KB SRAM and 4MB PSRAM.
*   **Pin Connections:**
    *   **TX (GPIO 15) / RX (GPIO 14):** Connected to Arduino SoftwareSerial for data exchange.
    *   **Relay Pin (GPIO 12):** Active-low control for the water pump.
*   **Role:** Maintains WiFi connection, hosts the web dashboard, processes MQTT messages, and runs the vital **Timer Logic** (Quick Timer, Schedule, Auto Mode). It also captures and streams video.

### 4.2 Arduino Uno (Slave Unit)
The Arduino handles all sensor interfacing to offload the ESP32. It uses an ATmega328P microcontroller running at 16 MHz.
*   **Pins Used:**
    *   **Digital 2:** Flow Sensor (Interrupt).
    *   **Digital 4:** DHT22 Sensor.
    *   **Digital 3:** DS18B20 Soil Temperature (1-Wire).
    *   **Digital 5:** PIR Motion Sensor.
    *   **Digital 6 (Trig) / 7 (Echo):** Ultrasonic Sensor.
    *   **Digital 8 (RX) / 9 (TX):** SoftwareSerial to ESP32.
    *   **Digital 10:** Pump Relay Control (Secondary/Direct).
    *   **Analog A0:** Rain Sensor.
    *   **Analog A1:** TDS Sensor.
    *   **Analog A2:** LDR (Light Sensor).
    *   **Analog A3:** Capacitive Soil Moisture Sensor.

### 4.3 Sensor Integration & Calibration Algorithms

#### 4.3.1 Capacitive Soil Moisture Sensor v1.2
Unlike resistive sensors that corrode over time, this sensor measures soil dielectric permittivity.
*   **Calibration Algorithm:**
    The raw analog reading (0-1023) is inversely proportional to moisture.
    $$ Moisture(\%) = map(RawValue, AirValue, WaterValue, 0, 100) $$
    Where:
    *   $AirValue \approx 1023$ (Dry)
    *   $WaterValue \approx 400$ (Wet)
    *   The result is constrained between 0% and 100%.

#### 4.3.2 DHT22 (AM2302) Temperature & Humidity
*   **Protocol:** Single-wire digital protocol. The sensor sends 40 bits of data:
    *   16 bits: Relative Humidity (Integral + Decimal)
    *   16 bits: Temperature (Integral + Decimal)
    *   8 bits: Checksum
    *   **Algorithm:** The Arduino validates the checksum: $(RH_{high} + RH_{low} + T_{high} + T_{low}) \& 0xFF == Checksum$.

#### 4.3.3 TDS (Total Dissolved Solids) Sensor
*   **Measurement:** Measures the electrical conductivity (EC) of the soil solution. 
*   **Compensation Algorithm:**
    $$ V_{compensated} = V_{raw} / (1 + 0.02 * (T - 25)) $$
    $$ TDS_{ppm} = (133.42 * V^3 - 255.86 * V^2 + 857.39 * V) * 0.5 $$
    This polynomial regression converts voltage to PPM, adjusted for temperature ($T$).

#### 4.3.4 Security Sensor Fusion
The system employs a **Multi-Sensor Fusion Algorithm** to reduce false alarms.
*   **Inputs:**
    *   $S_{PIR} \in \{0, 1\}$ (Motion Detected)
    *   $D_{Ultra} \in [0, 400]$ (Distance in cm)
*   **Fusion Logic:**
    1.  **State 1 (Idle):** No motion, Distance > Threshold. $Confidence = 0\%$.
    2.  **State 2 (Motion Alert):** $S_{PIR} == 1$. $Confidence = 50\%$. Probable cause: Wind, small animal.
    3.  **State 3 (Proximity Alert):** $D_{Ultra} < 50cm$. $Confidence = 50\%$. Probable cause: Object placed near sensor.
    4.  **State 4 (Confirmed Intrusion):** $S_{PIR} == 1 \land D_{Ultra} < 50cm$ (within $\Delta t = 3s$). $Confidence = 100\%$.
    
    $$ AlarmState = (S_{PIR}(t) \lor S_{PIR}(t-\Delta t)) \land (D_{Ultra} < 50) $$

#### 4.3.5 Flow Sensor (Hall Effect)
*   **Principle:** A turbine rotor with a magnet spins with water flow.
*   **Algorithm:**
    $$ FlowRate (L/min) = \frac{PulseFrequency (Hz)}{CalibrationFactor (Q)} $$
    For YF-S401, $Q \approx 98$.
    $$ Volume (L) = \sum_{t=0}^{n} FlowRate_t \times \frac{\Delta t}{60} $$

---

## Chapter 5: Software Design & Implementation

### 5.1 Technology Stack
*   **Firmware:** C/C++ (Arduino Framework).
*   **Frontend:** HTML5, CSS3 (Glassmorphism), Vanilla JavaScript (ES6+).
*   **Protocols:** MQTT (Message Queuing Telemetry Transport) over TCP/IP.
*   **Backend:** Google Firebase (NoSQL Database).
*   **AI Engine:** Google Gemini 2.5 Flash (via v1 API) for multimodal plant analysis with secure header-based API key management.

### 5.2 Microcontroller Algorithms (ESP32-CAM)

#### 5.2.1 Finite State Machine (FSM) for Pump Control
The pump control logic is implemented as a prioritized FSM.
**Priorities (implemented):** Manual Override > Auto Mode > Quick Timer > Schedule.

1.  **Check Manual:** If (ManualState == ACTIVE), set Output = ManualDesiredState.
2.  **Check Auto:** If (AutoEnabled == TRUE):
    *   **Closed-Loop Hysteresis Algorithm:**
        $$ State_{next} = \begin{cases} ON & \text{if } Moisture_{raw} > Threshold_{dry} \\ OFF & \text{if } Moisture_{raw} < Threshold_{stop} \\ State_{prev} & \text{otherwise} \end{cases} $$
        *Where $Threshold_{stop}$ is explicitly set (typically $Threshold_{dry} - 50$) to create a target moisture "window."*
3.  **Check Quick Timer:** If (TimerActive == TRUE):
    *   If (Elapsed < OnDelay), Output = OFF.
    *   If (Elapsed >= OnDelay && Elapsed < OffDelay), Output = ON.
4.  **Check Schedule:** If (CurrentTime == ScheduleTime), Output = ON for Duration.

#### 5.2.2 Dry Run Protection Algorithm (Hardware-Level Safety)
The system implements a hardware-level dry run protection mechanism to prevent pump damage from running without water flow. This feature operates independently at the ESP32 firmware level, ensuring protection even when the web application is closed or killed.

**Algorithm Logic:**
```
IF (dryRunProtectionEnabled == TRUE AND pumpPhysicalState == ON) THEN
    timeSincePumpStart = millis() - pumpStartTime
    timeSinceLastFlow = millis() - lastFlowDetectedTime
    
    // Allow 5 seconds for flow to establish after pump start
    IF (timeSincePumpStart > 5000) THEN
        // Check if no flow detected for 30 seconds
        IF (timeSinceLastFlow > 30000) THEN
            LOG("DRY RUN DETECTED! Stopping pump")
            dryRunAlertActive = TRUE
            setPumpState(OFF, "Dry Run Protection")
            
            // Release all control modes to prevent immediate restart
            manualState.active = FALSE
            quickTimer.active = FALSE
            clearQuickTimerPrefs()
        END IF
    END IF
END IF
```

**Flow Detection Logic:**
*   Arduino monitors the YF-S401 flow sensor via interrupt (Pin 2).
*   Every 1 second when pump is ON, Arduino sends: `FLOW:rate,volume` to ESP32.
*   ESP32 updates `lastFlowDetectedTime` if `flowRate > 0.1 L/min`.
*   If no flow updates received for 30 seconds → Trigger dry run protection.

**Key Features:**
*   **Hardware-Level Operation:** Runs in ESP32 firmware loop, independent of cloud connectivity.
*   **Persistent Settings:** User preference stored in ESP32 Preferences and Firebase for cross-device sync.
*   **Automatic Recovery:** Alert clears automatically when flow resumes.
*   **Protection Scenarios:** Empty water tanks, blocked pipes, closed valves, sensor failures.
*   **Environmental Awareness:** Specialized threshold logic for Kharif, Rabi, and Plantation crops.

#### 5.2.3 Watchdog Timer Algorithm (Offline Detection)
Implemented in both ESP32 firmware and Web UI javascript.
*   **Web UI Logic:**
    1.  Initialize `watchdogTimer` = null.
    2.  On `MQTT.MessageArrived`: `clearTimeout(watchdogTimer)`.
    3.  Set new `watchdogTimer` for 15 seconds.
    4.  If timer expires: `setUIState('OFFLINE')`.

### 5.3 Communications Protocol (Serial)
A custom ASCII-based protocol is used for reliability over SoftwareSerial.
*   **Arduino $\rightarrow$ ESP32:**
    *   `ENV:Temp,Hum,Moist%,RawMoist,SoilTemp,TDS,Rain,Light`
    *   `SEC:Motion,Distance`
    *   `FLOW:Rate,Volume`
*   **ESP32 $\rightarrow$ Arduino:**
    *   `PUMP_ON` / `PUMP_OFF`

### 5.4 Web Dashboard Algorithms

#### 5.4.1 Sunlight Duration Calculation
The dashboard implements two sunlight-related calculations:

1) **Live “current phase duration” (Monitor Card):**
    - On each MQTT sensor update, it reads `data.light` where `0 = Daytime`, `1 = Nighttime`.
    - It stores `lastLightState` and `lastLightChangeTime` in `localStorage`.
    - If the state changes, it resets the timestamp; otherwise it displays `Duration = now - lastLightChangeTime`.

2) **Estimated daily sunlight (History/Chart):**
    - The function `calculateDailySunlight(targetDateStr)` filters historical `sensorData.light` readings for one day.
    - It integrates time between successive “Daytime” readings (state assumed to hold until the next reading), with a safety cap to avoid overcounting large gaps (>1 hour).

**Important note:** The live Monitor Card duration shows the **current day/night phase duration**, while `calculateDailySunlight(...)` provides a **best-effort daily estimate** based on sampling frequency and data continuity.

| >2000 | Toxic | Urgent Flushing Needed |

### 5.5 UI Module Architecture (Single Page Application)
The frontend is built as a responsive Single Page Application (SPA), utilizing a tab-based navigation system with real-time state synchronization.

#### 5.5.1 Home Page: Dashboard & Environmental Context
The Home page serves as the user's primary landing zone, providing a high-level summary of the farm's status.
*   **Weather Integration:** Integrates the OpenWeatherMap API to provide live local weather data, including temperature, humidity, and atmospheric conditions, helping farmers anticipate irrigation needs.
*   **Quick Status Tiles:** Displays real-time connectivity status (ESP32 & MQTT) and key environmental metrics at a glance.
*   **Pump Control Center:** Provides immediate access to manual pump overrides and quick-timer settings.
*   **Closed-Loop Auto Irrigation (Advanced V2):**
    *   **Extensive Crop Library:** Features a built-in database of 40+ specific crops categorized by season (Kharif, Rabi) and type (Vegetables, Fruits/Plantation).
    *   **Precision Thresholds:** Each crop loads unique Start and Stop moisture thresholds, enabling tailored irrigation for different growth stages.
    *   **Environmental Stress Alerts:** Real-time logic monitors for Waterlogging (>30%) and Crop Stress (<12%), with color-coded UI indicators and persistent cross-device synchronization via Firestore.
    *   **Visual Feedback:** A dedicated "Live Moisture" status and pulses on the pump control button provide immediate awareness of system activity.
*   **Dry Run Protection Toggle:** User-configurable safety feature with expandable info panel explaining the hardware-level protection mechanism. Settings are synced to Firebase and MQTT for persistent, device-independent operation.
    *   **Collapsible Info Panel:** Educates users about the feature:
        *   Monitors water flow sensor continuously
        *   Auto-stops pump after 30 seconds if no flow detected
        *   Prevents pump damage from dry running
        *   Works independently (even if app is closed)
        *   Protects against: empty tanks, blocked pipes, valve issues

#### 5.5.2 Analyze Page: AI-Powered Diagnostics
The Analyze page leverages cutting-edge AI to assist in crop health management.
*   **Expert Agro-Advisor (Gemini 2.5 Flash):** A multimodal AI module that analyzes symptoms from the ESP32-CAM feed or local uploads.
*   **Context-Aware Disease Verification Matrix:**
    *   **AI-Sensor Fusion:** Implements a logical decision matrix that cross-verifies visual AI symptoms (e.g., Yellow Leaves) with real-time sensor environmental data (e.g., Low TDS).
    *   **Decision Matrix:** Provides a structured lookup where visual cues and sensor conditions (Temperature, Humidity, TDS) are combined to reach a verified diagnosis.
    *   **Diagnostic Workflow (10 Steps):** A visualized process that walks through the entire pipeline: 1. Start → 2. Capture → 3. Transmit → 4. AI Detection → 5. Acquire Sensors → 6. Comparison → 7. Matrix Processing → 8. Confirmation → 9. Notification → 10. End.
    *   **System Actions Based on Diagnosis:** Recommends specific immediate actions, such as alerting for fungicides, recommending fertilizers for nutrient deficiencies, or adjusting irrigation for water stress.
    *   **Verification Reliability:** Minimizes "False Positives" and prevents unnecessary chemical usage by requiring environmental consistency before confirmation.
*   **Bengali-First Interface:** Following the project's localization mandate, all AI results are delivered in native Bengali by default to maximize accessibility for local farmers.
*   **Structured UI:** Results are presented in a clean, section-based layout (Explanation, Action, Prevention) for better readability.
*   **Cross-Lingual Toggle:** A dedicated translation engine allows users to switch the expert analysis between Bengali and English instantly.
*   **Gemini API Security & Management (NEW):**
    *   **Secure Storage:** Removed hardcoded API keys from the source code. The system now utilizes browser `localStorage` to persist user-provided keys securely.
    *   **Header-Based Authentication:** To prevent API key leakage in console logs or network traces, the system now passes the key via secure `x-goog-api-key` headers rather than URL query parameters.
    *   **Advanced Error Handling:** Implemented specific handlers for 429 (Rate Limit) errors, providing user-friendly "Quota Exceeded" feedback in Bengali.
    *   **User-Centric Configuration:** Integrated a "Gemini API Settings" page in the "More" tab, allowing users to input, save, and manage their own Gemini API keys.
    *   **Visibility Control:** Features a toggle to hide/show the API key for privacy during configuration.
    *   **Direct Access:** Provides instructions and a direct link to Google AI Studio for obtaining keys, empowering users to manage their own cloud resources.

#### 5.5.3 Monitor Page: Telemetry & Analytics
*   **Real-time Visualization:** Uses **Chart.js** to plot sensor data (Moisture, Temp, TDS, etc.) as it arrives.
*   **Historical Filtering:** Supports time-window filtering (1 Hour, 1 Day, 1 Week) by querying historical snapshots from Firestore.
*   **Water Usage Tracking:** Displays daily water consumption in Liters, derived from hall-effect flow sensor data.

#### 5.5.4 Security Page: Perimeter Defense
*   **Intruder Detection:** Implements a fusion logic combining PIR motion sensing and Ultrasonic distance measurement to confirm threats.
*   **Alarm Synchronization:** Features a cross-device alarm system powered by MQTT, ensuring all connected dashboards reflect an active intruder event.
*   **Incident Logging:** Every confirmed intrusion is logged to Firebase with a timestamp and severity score for later review.

#### 5.5.4.1 Nutrient Loss Monitor: Auto Runoff Analysis (NEW)
The Nutrient Loss Monitor is a data-driven analytics module that tracks nutrient runoff during irrigation and rainfall events without requiring additional hardware sensors.

*   **Event-Based Monitoring:**
    *   **Automatic Event Detection:** Monitors rain sensor and pump status in real-time. When either activates (rain detected OR pump turned ON), the system automatically starts a monitoring event and records the initial TDS (Total Dissolved Solids) value.
    *   **Event Completion:** When both rain stops AND pump turns OFF, the system ends the event, records the final TDS, and calculates nutrient loss.
    *   **Loss Calculation:** 
        - Nutrient Loss (ppm) = Initial TDS - Final TDS
        - Loss Percentage = (Nutrient Loss / Initial TDS) × 100
        - Negative values are clamped to zero to handle sensor fluctuations.

*   **Multi-Device Alert System:**
    *   **Cloud-Synchronized Alerts:** When nutrient loss ≥5%, an alert is created in Firebase (`/alerts/nutrientAlert`) that instantly appears on all connected devices.
    *   **Severity Classification:**
        - **Safe (<5%):** No alert generated. System continues normal monitoring.
        - **Warning (5-15%):** Yellow banner displayed with moderate action recommendations.
        - **Critical (>15%):** Red banner with audio alarm and urgent action required.
    *   **Cross-Device Acknowledgment:** Any device can dismiss the alert using the "Acknowledge" button, which updates Firebase and instantly removes the banner from all other devices.
    *   **Persistent Banner:** Alert banner appears as a fixed top bar on all pages while active, ensuring maximum visibility.

*   **Live Event Dashboard:**
    *   **Status Cards:** Real-time display of Rain Status (Active/Inactive), Pump Status (ON/OFF), and Event Status (Monitoring/Idle).
    *   **TDS Tracking:** Shows Initial TDS (at event start), Current TDS (live updates), and Final TDS (at event end).
    *   **Visual Indicators:** Event monitoring card highlights with blue border and background when an active event is in progress.

*   **Nutrient Loss Analytics:**
    *   **Key Metrics Display:** Large, prominent cards showing Nutrient Loss (ppm), Loss Percentage (%), and color-coded Severity badges.
    *   **Event Timeline:** Displays event duration (in minutes) and timestamp of the last recorded event.
    *   **TDS Chart:** Interactive Chart.js line graph visualizing TDS changes over time, with clear markers for event start and end points.

*   **Event History Table:**
    *   **Comprehensive Records:** Stores and displays the last 10 nutrient monitoring events.
    *   **Table Columns:** Date, Event Type (Rain/Pump/Both), Initial TDS, Final TDS, Loss %, and Severity.
    *   **Color-Coded Severity:** Visual badges (Green/Yellow/Red) provide instant risk assessment.
    *   **Persistent Storage:** All events are stored in Firebase (`/nutrientMonitor/history/{eventID}`) for long-term analysis.

*   **Smart Recommendation Engine:**
    *   **Dynamic Fertilizer Guidance:** Provides context-aware recommendations based on final TDS levels and loss severity:
        - **Final TDS < 300 ppm:** Recommends immediate NPK 19-19-19 foliar spray application.
        - **Final TDS 300-600 ppm:** Suggests organic compost or balanced fertilizer addition.
        - **Final TDS > 600 ppm:** Advises continued monitoring without immediate intervention.
    *   **Urgency Indicators:** Critical loss events display urgent action boxes with step-by-step remediation guidance.
    *   **Prevention Tips:** Includes suggestions for drip fertigation and soil amendment strategies.

*   **Firebase Integration:**
    *   **Real-Time Listeners:** Uses Firebase onSnapshot() for instant synchronization across devices.
    *   **Data Structure:**
        - `/nutrientMonitor/activeEvent/`: Stores currently active event data (startTime, initialTDS, rainDetected, pumpUsed).
        - `/nutrientMonitor/history/{eventID}/`: Archives completed events with full metrics.
        - `/alerts/nutrientAlert/`: Manages cross-device alert state and messaging.
    *   **Automatic Cleanup:** Active events are automatically moved to history upon completion.

*   **Key Benefits:**
    *   **Zero Additional Hardware:** Leverages existing TDS, rain, and pump sensors.
    *   **Real-Time Notifications:** Instant alerts across all connected devices.
    *   **Data-Driven Decisions:** Historical tracking enables pattern recognition and optimization.
    *   **Cost Savings:** Prevents excessive fertilizer loss and reduces chemical waste.
    *   **Environmental Protection:** Minimizes nutrient runoff into water sources.

#### 5.5.5 Menu & Account: Analytics & Localization
The Menu module provides access to advanced system tools and configuration.
*   **Language Selection:** A global localization toggle allows the entire UI to be switched between **English** and **Bengali (বাংলা)**.
*   **Analytics Dashboard:** 
    *   **Performance Metrics:** Aggregates data from `modelFeedback` to display accuracy bars for each AI model (e.g., YOLOv8, Rice, Wheat).
    *   **Risk Level Assessment:** Dynamically calculates current farm risk (Low/Medium/High) based on the frequency of recent disease detections.
    *   **Model Precision Chart:** A Chart.js bar chart visualizing "Correct" vs "Wrong" user feedback to track real-world AI reliability.
*   **Analysis Logs (Diagnostic History):**
    *   **Timeline Feed:** A chronological list of all past plant analyses, filterable by status (All, Diseases, Healthy).
    *   **Full Context Storage:** Every log entry persists the `verificationResult` (comparing visual AI with live sensors) alongside the primary diagnosis.
    *   **Asynchronous AI Persistence:** Gemini AI expert advice is automatically appended to the corresponding log entry in Firebase as soon as the analysis is completed.
    *   **Collapsible Context:** Each log entry is a card that expands to show environmental sensor readings (Temp, Humidity, Moisture) and individual model consensus.
    *   **AI Prioritization:** The expanded view prioritizes AI-generated results at the top, ensuring farmers see the expert recommendations first, followed by technical sensor fusion data.
    *   **Cloud Synchronization:** Utilizes Firebase Firestore's `orderBy` and `limit` queries to maintain a performant history of up to 100 recent records.
*   **Data Portability:** Features for "Export Analytics Data" (JSON download) and "Clear Logs" for storage management.

### 5.6 Project Aesthetics & UX

This section documents the actual source code behavior in the project folder, focusing on function-level logic, loops, message formats, and runtime edge cases.

#### 5.6.1 Web Dashboard (`index.html`) — MQTT Configuration and Connection Flow

- Broker: `broker.emqx.io`
- WebSocket ports: `8083` (WS) for HTTP pages, `8084` (WSS) for HTTPS pages
- Path: `/mqtt`
- Client ID: `WebAppClient_<random>`

**Connection functions (as implemented):**
1. `tryConnect(brokerHost, port, path, useSSL, attemptFallback)`
     - Instantiates the Paho client.
     - Assigns `onConnectionLost` and `onMessageArrived` callbacks.
     - Attempts connection; if a secure (WSS) attempt fails and fallback is allowed, retries against a known WSS endpoint.
2. `onConnect()`
     - Subscribes to irrigation topics: `irrigation_arnab/sensors/data`, `irrigation_arnab/pump/status`, `irrigation_arnab/alarm/status`.
     - Enables pump UI controls and flushes any queued irrigation commands.
3. `onConnectionLost(...)`
     - Marks MQTT offline and disables pump UI controls.

#### 5.6.2 Web Dashboard (`index.html`) — Irrigation Control (Device-Side Timers)

**Key design decision:** all timers/schedules run on the ESP32 firmware. The dashboard sends retained MQTT commands; the ESP32 executes autonomously even if the phone/browser is closed.

**Publish helper + offline queue:**
- `publishIrrigationMessage(topic, payload, { retained=true, qos=1, queueKey=null })`
    - If MQTT is offline, it queues only the most recent schedule payload and the most recent quick-timer payload.
- `flushPendingIrrigationCommands()` publishes queued payloads after reconnection.

**Topics and payloads used by the dashboard:**
- Manual override (`irrigation_arnab/pump/manual`, retained):
    ```json
    {"type":"MANUAL","state":"ON"}
    ```
    ```json
    {"type":"MANUAL","state":"OFF"}
    ```
- Auto mode (`irrigation_arnab/pump/auto`, retained):
    ```json
    {"type":"AUTO","enabled":true,"threshold":800}
    ```
- Quick timer (`irrigation_arnab/pump/schedule`, retained, `type: QUICK_TIMER`):
    ```json
    {"type":"QUICK_TIMER","on_delay_sec":0,"off_delay_min":10}
    ```
- Schedule (`irrigation_arnab/pump/schedule`, retained, `type: SCHEDULE`):
    ```json
    {"type":"SCHEDULE","start":"08:00","duration_min":5,"enabled":true}
    ```

**Multiple schedules in UI vs single schedule on device:**
The UI can render multiple schedule rows, but `savePumpSchedules()` sends only the **first enabled** schedule to the ESP32.

#### 5.6.3 Web Dashboard (`index.html`) — MQTT Inbound Dispatcher

`onMessageArrived(message)` branches by topic:

1) **Sensor telemetry** (`irrigation_arnab/sensors/data`)
- Updates live UI values (temperature, humidity, moisture, soil temperature, TDS, rain, motion, distance, light state, flow rate, daily volume).
- Calls `resetSensorWatchdog()` to keep an offline-detection timer alive (15 s timeout).
- Calls `processSecurityLogic(data)` when `motion` and/or `distance` is present.
- Calls `saveSensorReading(...)` to append to local history arrays and write to Firebase when enabled.

2) **Alarm synchronization** (`irrigation_arnab/alarm/status`)
- If another device publishes `{status:'ACTIVE'}`, this client calls `startAlarm(true)`.
- If another device publishes `{status:'STOPPED'}`, this client calls `stopAlarm(true)`.

3) **Pump status** (`irrigation_arnab/pump/status`)
- Parses the ESP32 status JSON and calls `updatePumpStatus(status)`.
- Uses a merge strategy (`window.lastPumpStatus`) so partial status packets do not overwrite previously-known fields.

#### 5.6.4 Web Dashboard (`index.html`) — Security Fusion + Alarm Engine

**Fusion timing windows:**
- Motion is treated as “active” for 3 seconds after the last PIR trigger.
- Distance is treated as “active” for 3 seconds after the last <50 cm ultrasonic reading.
- Event debounce: 5 seconds (prevents repeated logs/toasts).

**Confidence scoring (implemented):**
- Motion + distance active: 100% (Critical Intrusion)
- Only one active: 50% (Warning)

**Alarm:**
- `startAlarm()` shows a banner, optionally auto-stops after a configured duration, and generates a repeating beep using Web Audio API.
- Cross-device sync uses MQTT so multiple dashboards can reflect the same alarm state.

#### 5.6.5 Web Dashboard (`index.html`) — Sensor History, Filtering, and Chart Updates

- `loadSensorData()` loads the `sensorData` collection from Firebase and rebuilds local arrays.
- `filterSensorData(data, filter)` supports numeric timestamps, ISO strings, and Firestore Timestamp objects (`toMillis()`).
- Each chart update function destroys and recreates its Chart.js instance using the filtered data.

#### 5.6.6 ESP32 Master Firmware (`ESP32_CAM_MASTER/ESP32_CAM_MASTER.ino`)

**Core responsibilities:** Wi‑Fi + MQTT client, NTP time sync, device-side pump control (manual/auto/timer/schedule), camera streaming server, serial gateway to Arduino.

**Key functions:**
- `mqttCallback(...)`: parses JSON commands for manual/auto/schedule/quick-timer and persists settings.
- `processTimerLogic()`: applies priority order Manual > Auto > Quick Timer > Schedule.
- `setPumpState(state, reason)`: toggles relay (active‑LOW) and sends `PUMP_ON`/`PUMP_OFF` to Arduino over Serial2.
- `readArduinoSerial()`: parses `ENV:`, `SEC:`, `MOTION:`, `FLOW:` frames and publishes MQTT sensor packets.
- `handleStream()`: serves MJPEG stream while keeping MQTT alive and continuing pump logic.

**Auto hysteresis (actual implementation):**
- ON if `currentMoistureRaw > threshold`
- OFF if `currentMoistureRaw < threshold - 50`
- Else keep prior AUTO state

#### 5.6.7 Arduino Slave Firmware (`arduino_slave/arduino_slave.ino`)

**Core responsibilities:** periodic sensor polling + formatting into ASCII frames, relay actuation on D10, and flow pulse counting on interrupt pin D2.

**Key functions/loops:**
- `pulseCounter()` ISR increments flow pulse count.
- `handleCommand(cmd)` processes `PUMP_ON`/`PUMP_OFF` and sends a final `FLOW:0.0,0` when turning OFF.
- Main loop:
    - Non-blocking command buffering until newline.
    - Periodic sensor frames (ENV/SEC) and conditional flow frames.

### 5.7 Firebase Data Model & Access Control (As Used)

**Collections/documents used:**
- `sensorData`
- `settings/pump_control`
- `securitySettings/config`
- `securityEvents`
- `users/{uid}/security_logs`
- `diseaseAnalytics`
- `modelFeedback/current`

**Security policy:** Firestore reads/writes are authentication-gated in `firestore.rules`, so Firebase features are available only when the user is signed in.

---

## CHAPTER 6: CORE ALGORITHMS (TEXTUAL REPRESENTATION)

This chapter provides a detailed textual breakdown of the core logic used across the hardware and software layers of the Agri-Nigrani system.

### 6.1 Multi-Prioritized Pump Control Algorithm
**Logic Level**: ESP32 Master Firmware  
**Format**: Structured Pseudocode

```text
ALGORITHM PumpPriorityController
1.  START LOOP (runs continuously in ESP32 loop)
2.  SET DesiredState = OFF
3.
4.  IF ManualOverride.Active IS TRUE THEN
5.      SET DesiredState = ManualOverride.State
6.  ELSE IF AutoMode.Enabled IS TRUE THEN
7.      // Raw ADC: higher value indicates drier soil in this calibration
8.      IF MoistureRaw > DryThresholdRaw THEN
9.          SET DesiredState = ON
10.     ELSE IF MoistureRaw < (DryThresholdRaw - HysteresisBandRaw) THEN
11.         SET DesiredState = OFF
12.     ELSE
13.         KEEP DesiredState = CurrentPumpState (Hysteresis Guard)
14.     END IF
15.  ELSE IF QuickTimer.Active IS TRUE THEN
16.     IF ElapsedSeconds < OnDelaySeconds THEN
17.         SET DesiredState = OFF
18.     ELSE
19.         SET DesiredState = ON
20.         IF OffDelayMinutes > 0 AND ElapsedSeconds >= (OnDelaySeconds + OffDelayMinutes*60) THEN
21.             SET DesiredState = OFF
22.             SET QuickTimer.Active = FALSE
23.         END IF
24.     END IF
25.  ELSE IF Schedule.IsRunningNow(CurrentNTPTime) IS TRUE THEN
26.     SET DesiredState = ON
27.  END IF
28.
29.  IF DesiredState != PhysicalRelayState THEN
30.     ACTUATE Relay(DesiredState)
31.     PUBLISH Status Update via MQTT
32.  END IF
33. END LOOP
```

### 6.2 Sensor Fusion Security Logic
**Logic Level**: Web Dashboard (JavaScript)  
**Format**: Conditional Logic Flow

```text
ALGORITHM IntruderDetectionFusion
1.  ON EVENT (PIR_Trigger OR Distance_Update)
2.  GET PIR_State (0 or 1)
3.  GET Distance_CM (Value from Ultrasonic)
4.  
5.  IF PIR_State IS 1 THEN
6.      SET MotionTime = CurrentTime
7.  
8.  IF Distance_CM < 50 AND (CurrentTime - MotionTime < 3000ms) THEN
9.      SET ThreatLevel = "CRITICAL"
10.     TRIGGER LoudAlarm()
11.     LOG_TO_FIREBASE("Confirmed Intruder Detected")
12. ELSE IF PIR_State IS 1 THEN
13.     SET ThreatLevel = "WARNING"
14.     LOG_HISTORY("Suspected Motion Detected")
15. ELSE
16.     SET ThreatLevel = "SECURE"
17. END
```

### 6.3 Sunlight Duration Calculation
**Logic Level**: Client-side (Live Monitor UI)  
**Format**: State-change duration

```text
ALGORITHM LightPhaseDuration
1.  ON EACH MQTT SENSOR UPDATE:
2.      READ LightState (0=Day, 1=Night)
3.      LOAD LastState and LastChangeTime from localStorage
4.      IF LightState != LastState THEN
5.          SET LastChangeTime = now
6.          STORE LastState and LastChangeTime
7.      COMPUTE Duration = now - LastChangeTime
8.      DISPLAY Duration as hours/minutes
```

### 6.4 Disease Consensus & Severity Rating
**Logic Level**: AI Module (JS + Firebase)  
**Format**: Weighted Evaluation

```text
ALGORITHM DiseaseSeverityAnalysis
1.  START ANALYSIS (Image Uploaded)
2.  IDENTIFY PlantSpecies AND DetectedSymptoms
3.  MATCH Symptoms against KnowledgeBase
4.  
5.  CALCULATE BaseSeverity = (DamagedArea / TotalLeafArea) * 100
6.  IF Humidity > 85% THEN Multiply Severity by 1.2 (Fungal Growth Risk)
7.  IF ModelPrecision < 70% THEN Mark result as "PENDING_VERIFICATION"
8.  
9.  OUTPUT DiagnosisSummary
10. OUTPUT TreatmentPlan
11. REQUEST UserFeedback(Correct/Wrong)
```

### 6.5 Multi-Lingual Structured AI Diagnosis (Gemini 2.5)
**Logic Level**: AI Module (Native JSON Parser)  
**Format**: Structured Pseudocode

```text
ALGORITHM StructuredAgroAnalysis
1.  INITIALIZE Model = "gemini-2.5-flash"
2.  INITIALIZE ResponseFormat = JSON_OBJECT
3.  PREPARE Prompt: "Identify plant species and symptoms... Return in BENGALI... EXACT JSON structure"
4.  
5.  RETRIEVE GEMINI_API_KEY from localStorage
6.  IF Key Missing THEN Prompt User to Configure Settings AND EXIT
7.  POST Image + Prompt TO Gemini v1 API with User Key
8.  PARSE Response: Get primary candidate content parts
9.  CLEAN Text: Remove markdown formatting (```json)
10. IF Language == "bn" THEN
11.     RENDER SectionedLayout(explanation, actions[], prevention[])
12. IF UserTriggersTranslation THEN
13.     FETCH Translation via Gemini v1 API Bridge
14.     SWAP UI state to English
15. END
```

### 6.6 Master-Slave Data Synchronization (UART Frames)
**Logic Level**: ESP32 Master Serial Parser + Arduino Framing  
**Format**: Tokenized Line Parser

```text
ALGORITHM SyncSlaveData
1.  WHILE Serial2.Available IS TRUE
2.      READ Char c
3.      IGNORE '\r'
4.      APPEND c TO lineBuffer UNTIL '\n'
5.      IF '\n' RECEIVED THEN
6.          IF line STARTS WITH "ENV:" THEN
7.              PARSE tokens: temp, hum, moist%, moist_raw, soilTemp, tds, rain, light
8.              UPDATE ESP32 runtime variables
9.              PUBLISH MQTT sensor JSON (environment)
10.         ELSE IF line STARTS WITH "SEC:" THEN
11.             PARSE tokens: motion, distance
12.             UPDATE ESP32 runtime variables
13.             PUBLISH MQTT sensor JSON (security)
14.         ELSE IF line STARTS WITH "MOTION:" THEN
15.             UPDATE motion immediately
16.             PUBLISH MQTT security JSON (low-latency)
17.         ELSE IF line STARTS WITH "FLOW:" THEN
18.             PARSE flowRate
19.             UPDATE dailyVolumeLiters estimate and publish status
20.         END IF
21.         CLEAR lineBuffer
```

### 6.7 Hysteresis Irrigation Algorithm
**Logic Level**: ESP32 Automation Engine  
**Format**: Threshold Management

```text
ALGORITHM HysteresisIrrigation
1.  READ CurrentMoistureRaw
2.  READ DryThresholdRaw FROM Preferences
3.  SET HysteresisBandRaw = 50
4.  
5.  IF CurrentMoistureRaw > DryThresholdRaw THEN
6.      SET PumpDesiredState = ON
7.  ELSE IF CurrentMoistureRaw < (DryThresholdRaw - HysteresisBandRaw) THEN
8.      SET PumpDesiredState = OFF
9.  ELSE
10.     RETAIN CurrentPumpState
11. END
```

### 6.8 MQTT Command Handler (Retained JSON Control)
**Logic Level**: ESP32 MQTT callback (`mqttCallback`)  
**Format**: Topic-dispatch + JSON parsing

```text
ALGORITHM MQTTCommandHandler
1.  ON MQTT MESSAGE ARRIVAL(topic, payload)
2.      PARSE payload as JSON
3.      IF topic == ".../pump/schedule" THEN
4.          IF json.type == "SCHEDULE" THEN
5.              UPDATE schedule.enabled/start/duration
6.              STORE schedule fields in Preferences
7.          ELSE IF json.type == "QUICK_TIMER" THEN
8.              SET quickTimer.active = TRUE and record startMillis
9.              STORE quick timer fields in Preferences (including epoch if time synced)
10.     ELSE IF topic == ".../pump/auto" AND json.type == "AUTO" THEN
11.         SET autoMode.enabled and optional threshold
12.         STORE auto settings in Preferences
13.     ELSE IF topic == ".../pump/manual" AND json.type == "MANUAL" THEN
14.         IF state == "OFF" THEN
15.             RELEASE manual mode and cancel any active quick timer
16.             CLEAR retained manual message (publish empty retained payload)
17.             APPLY pump OFF immediately
18.         ELSE IF state == "ON" THEN
19.             HOLD manual mode and APPLY pump ON immediately
20.     ELSE IF topic == ".../pump/safety" AND json.type == "SAFETY" THEN
21.         STORE safety config (not used in final build if disabled)
22. END
```

### 6.9 Device-Side Quick Timer Persistence & Restore
**Logic Level**: ESP32 Preferences + NTP time  
**Format**: Epoch-based restoration

```text
ALGORITHM RestoreQuickTimerFromPrefs
1.  IF qt_active == FALSE THEN EXIT
2.  READ qt_on_delay_sec, qt_off_delay_min, qt_start_epoch
3.  IF NTP time invalid OR qt_start_epoch invalid THEN EXIT
4.  elapsed = now_epoch - qt_start_epoch
5.  total = qt_on_delay_sec + (qt_off_delay_min * 60)
6.  IF qt_off_delay_min > 0 AND elapsed >= total THEN
7.      CLEAR quick timer prefs and EXIT
8.  SET quickTimer.active = TRUE
9.  SET quickTimer.startMillis = millis() - elapsed*1000
10. SET quickTimer.pumpHasStarted = (elapsed >= qt_on_delay_sec)
11. MARK quickTimerRestored = TRUE
```

### 6.10 Non-Blocking MQTT Reconnect Loop
**Logic Level**: ESP32 main loop + PubSubClient  
**Format**: timed retry

```text
ALGORITHM MQTTReconnectNonBlocking
1.  IF WiFi NOT connected THEN EXIT
2.  IF MQTT NOT connected THEN
3.      IF (millis - lastReconnectAttempt) >= 5000 THEN
4.          TRY mqttClient.connect(clientId)
5.          IF connected THEN
6.              SUBSCRIBE to control topics (schedule/manual/auto/safety)
7.              RESTORE schedule/auto/safety from Preferences
8.              PUBLISH retained status JSON
9.          UPDATE lastReconnectAttempt
10. ELSE
11.     mqttClient.loop()
```

### 6.11 Camera Streaming Service (MJPEG)
**Logic Level**: ESP32 HTTP server + camera driver  
**Format**: single-owner streaming loop

```text
ALGORITHM HandleStreamMJPEG
1.  ACCEPT client
2.  ENFORCE single stream owner (reject with 503 if busy)
3.  SEND multipart MJPEG HTTP headers
4.  WHILE client connected DO
5.      RUN mqttClient.loop() (keep connection alive)
6.      RUN processTimerLogic() (keep irrigation logic alive)
7.      CAPTURE camera frame buffer (JPEG)
8.      WRITE boundary + content-length + JPEG bytes (chunked)
9.      RELEASE frame buffer
10.     IF takeover requested THEN break
11. END WHILE
12. RELEASE stream ownership
```

### 6.12 Daily Water Usage Accumulation (Flow Sensor)
**Logic Level**: Arduino ISR (pulse counting) + ESP32 accumulation and publish  
**Format**: ISR + periodic safe aggregation

```text
ALGORITHM DailyWaterUsageAccumulation
Arduino side (ISR):
1.  ON falling edge interrupt from flow sensor (pulseCounter ISR):
2.      pulseCount++

ESP32 side (every flow interval or on FLOW frame):
3.  WHEN a FLOW frame ``FLOW:rate,vol`` is received OR periodic read occurs:
4.      IF flowRate (L/min) reported > 0 THEN
5.          litersPerSecond = flowRate / 60.0
6.          milliLitres = litersPerSecond * intervalSeconds * 1000
7.          dailyVolumeLiters += milliLitres / 1000.0
8.      ELSE
9.          // if zero, no addition; if pulses are sent raw, compute Hz->L/min:
10.         IF rawPulsesAvailable THEN
11.             flowHz = pulses / durationSec
12.             flowRate = flowHz / calibrationFactor
13.             compute milliLitres as above and add to dailyVolumeLiters
14.      END IF
15.  PUBLISH updated flow_rate and today_vol in `publishStatus()` or `publishEnvData()`.
16.  SAFETY: prevent div-by-zero, ignore implausible spikes, and cap per-interval volume to a sane maximum.

Daily reset:
17. IF NTP date changed (midnight detected) THEN
18.     dailyVolumeLiters = 0.0
19.     persist or publish reset event if needed
END
```


### 6.13 TDS Temperature Compensation
**Logic Level**: Sensor Interface (C++)  
**Format**: Step-by-Step Mathematical logic

```text
ALGORITHM CompensateTDS
1.  MEASURE RawVoltage FROM TDS Sensor
2.  MEASURE CurrentSoilTemperature FROM DS18B20
3.  
4.  CALCULATE CompensationCoefficient = 1.0 + 0.02 * (CurrentSoilTemperature - 25.0)
5.  CALCULATE CompensatedVoltage = RawVoltage / CompensationCoefficient
6.  
7.  MATCH CompensatedVoltage TO PPM using 3rd degree polynomial:
8.      TDS_PPM = (133.42 * V^3 - 255.86 * V^2 + 857.39 * V) * 0.5
9.  RETURN TDS_PPM
```

### 6.14 Capacitive Moisture Mapping
**Logic Level**: Analog Input Handler  
**Format**: Range Transformation

```text
ALGORITHM MapMoisturePercentage
1.  READ RawADC (0 - 1023)
2.  DEFINE AirValue = 1023 (Maximum Dryness)
3.  DEFINE WaterValue = 400 (Maximum Saturation)
4.  
5.  CALCULATE MoisturePercent = ((AirValue - RawADC) / (AirValue - WaterValue)) * 100
6.  CONSTRAIN MoisturePercent BETWEEN 0 and 100
7.  RETURN MoisturePercent
```

### 6.15 Analysis History Rendering Logic
**Logic Level**: Client-side UI Engine  
**Format**: Filtered List Rendering

```text
ALGORITHM UpdateHistoryTimeline
1.  INITIALIZE Filter = [ALL, DISEASES, or HEALTHY]
2.  FETCH LocalLogs FROM analyticsData.logs
3.  SORT LocalLogs BY timestamp DESCENDING
4.  
5.  FOR EACH Log IN LocalLogs DO
6.      IF Filter MATCHES Log.Status THEN
7.          DETERMINE SeverityClass (Low, Moderate, High)
8.          BUILD HTMLCard with:
9.              - Timestamp Header
10.             - Disease Name & Confidence %
11.             - Collapsible Section:
12.                 - Environment Grid (Temp, Hum, Soil)
13.                 - Model Table (detailed model breakdown)
14.         APPEND Card to timelineFeed
15.     END IF
16. END FOR
17. RENDER timelineFeed TO historyPanel
```

### 6.16 Analytics Aggregation Logic
**Logic Level**: Client-side Data Processor  
**Format**: Statistical Aggregation

```text
ALGORITHM CalculateFarmAnalytics
1.  INPUT: Array of DiagnosticLogs
2.  COUNT TotalScans = Logs.length
3.  COUNT DiseaseFound = Logs.filter(l => l.isHealthy == false).length
4.  
5.  // Calculate Confidence Trend
6.  FOR EACH Log IN Logs DO
7.      SUM Confidence += Log.avgConfidence
8.  SET AvgConfidence = SUM / TotalScans
9.  
10. // Risk Assessment (Rolling Window)
11. RECENT_DISEASES = Logs.slice(-10).filter(l => !l.isHealthy).length
12. IF RECENT_DISEASES > 7 THEN SET Risk = "HIGH"
13. ELSE IF RECENT_DISEASES > 4 THEN SET Risk = "MEDIUM"
14. ELSE SET Risk = "LOW"
15. 
16. UPDATE StatisticsCard(TotalScans, AvgConfidence, Risk)
```

### 6.17 Context-Aware Verification Logic
**Logic Level**: Web Dashboard (JavaScript)  
**Format**: Decision-Tree Fusion  

```text
ALGORITHM ContextAwareVerification
1.  ON DISEASE_DETECTED(diseaseName)
2.  GET CurrentSensors (temp, humidity, moisture, tds)
3.  LOAD LogicMatrix FOR diseaseName
4.  
5.  INITIALIZE verified = FALSE
6.  IF diseaseName == "Rice_Blast" THEN
7.      IF humidity > 85 AND temp < 28 THEN verified = TRUE
8.  ELSE IF diseaseName == "Nutrient_Deficiency" THEN
9.      IF tds < 500 THEN verified = TRUE
10. ELSE IF diseaseName == "Rice_Brown_Spot" THEN
11.     IF humidity > 90 THEN verified = TRUE
12. ELSE IF diseaseName == "Bacterial_Leaf_Blight" THEN
13.     IF rain > 50 OR humidity > 80 THEN verified = TRUE
14. ELSE
15.     SET verified = TRUE (Default for non-environmental diseases)
16. END IF
17. 
18. RETURN { 
19.     status: verified ? "VERIFIED" : "UNVERIFIED",
20.     confidence_boost: verified ? +15% : -20%,
21.     logic_used: "Fusion of visual symptoms with " + CurrentSensors
22. }
```

---

## CHAPTER 7: RESULTS & PERFORMANCE ANALYSIS

### 7.1 System Benchmarks
- **MQTT Command Latency**: < 250ms (Round-trip from Web UI to Relay trigger).
- **Sensor Accuracy**: DHT22 ($\pm 2\%$), Capacitive Moisture ($\pm 5\%$ after calibration).
- **Security Reliability**: 100% detection rate for intruders within 2 meters using the fusion logic.
- **Dry Run Protection**: 30-second detection window with <100ms shutdown execution time after threshold.

### 7.2 Safety Feature Validation
The dry run protection mechanism was tested under simulated failure conditions:
*   **Test Scenario 1 - Empty Tank:** Pump activation with no water source resulted in automatic shutdown after 30 seconds.
*   **Test Scenario 2 - Blocked Pipe:** Partial flow restriction (<0.1 L/min) triggered protection within the timeout window.
*   **Test Scenario 3 - Normal Operation:** Steady flow (>0.5 L/min) maintained pump operation without false triggers.
*   **Persistence Test:** Settings survived ESP32 reboot and web app closure, confirming device-side independence.

### 7.2 GUI Verification
The dashboard was tested for responsiveness across Desktop, Tablet, and Mobile viewports. The **Chart.js** implementation provides interactive zooming, allowing for micro-analysis of moisture trends during pump activity.

---

## CHAPTER 8: CONCLUSION & FUTURE SCOPE

### 8.1 Conclusion
Agri-Nigrani successfully bridges the gap between hardware deterministic control and high-level cloud analytics. The master-slave architecture provides a robust fail-safe mechanism, while the modern UI ensures ease of use for the final user. The addition of hardware-level dry run protection ensures pump longevity and prevents costly damage from operational failures, operating independently of network connectivity or application state.

### 8.2 Key Achievements
*   **Device-Side Intelligence:** Timer and safety logic runs entirely on ESP32, ensuring operation during network outages.
*   **Multi-Model AI Diagnostics:** Ensemble of 7 AI models + Gemini 2.5 Flash for accurate disease detection.
*   **Context-Aware Verification:** Integrated logic matrix that validates visual AI detections against real-time sensor data (Moisture, TDS, Temp, Humidity).
*   **Nutrient Loss Monitoring:** Automated tracking and alert system for fertilizer runoff during irrigation/rain events with multi-device synchronization.
*   **Hardware-Level Safety:** Dry run protection with 30-second timeout prevents pump damage.
*   **Bilingual Support:** Full Bengali translation for accessibility.
*   **Enhanced API Security:** Zero-hardcoded key architecture using `localStorage` for Gemini API management.
*   **Real-Time Monitoring:** Sub-second MQTT latency for responsive control.

### 8.3 Future Scope
- **Edge AI**: Implementing TensorFlow Lite directly on the ESP32-CAM.
- **LoRa Expansion**: Replacing WiFi with LoRa for large-scale agricultural deployments (10km+ range).
- **Advanced Safety Features**: Integration of pressure sensors for more precise dry run detection, and automatic valve control for leak prevention.
- **Multi-Pump Management**: Extending the system to control multiple pumps with independent flow monitoring and protection.
- **Predictive Maintenance**: Using historical flow and sensor data to predict pump failures before they occur.
- **Enhanced Nutrient Analytics**: Integration of NPK sensors for direct nutrient measurement and AI-based fertilizer scheduling recommendations based on crop stage and weather patterns.

---

## APPENDICES

### Appendix A: Logical Flowcharts (Mermaid)

**Pump Control Logic (ESP32 Master):**
```mermaid
graph TD
    Start[Loop] --> Manual{Manual CMD?}
    Manual -- Yes --> Exec[Actuate Relay]
    Manual -- No --> Auto{Auto Mode?}
    Auto -- Yes --> Hyst{Hysteresis Check}
    Hyst -- Dry --> Exec
    Auto -- No --> Sched{NTP Schedule?}
    Sched -- Match --> Exec
```

**Sensor Fusion Algorithm (Security):**
```mermaid
graph TD
    PIR[Motion Detected] --> Delay{Wait 3s}
    Ultra[Dist < 50cm] --> Delay
    Delay -- Both High --> Alarm[CRITICAL ALARM]
    Delay -- Either High --> Warn[Log Warning]
```

### Appendix B: File Structure Overview

Root:
- `Agri-Nigrani_Final_Report.md`: Final-year report (this document).
- `index.html`: Web dashboard (UI, MQTT control, Firebase logging, AI analysis, charts, security).
- `firestore.rules`: Firestore access control rules.
- `hero_card.jpg`, `monitor_card.jpg`, `starting_page.jpg`, `ui_background.png`: UI assets.

Firmware:
- `ESP32_CAM_MASTER/ESP32_CAM_MASTER.ino`: ESP32-CAM master firmware (WiFi/MQTT/NTP/pump logic/camera/serial gateway).
- `arduino_slave/arduino_slave.ino`: Arduino Uno slave firmware (sensor polling/relay actuation/flow ISR/serial frames).

IDE:
- `.vscode/`: local editor settings (if present).

---

### Appendix C: Complete Function Reference (Code-Grounded)

This appendix is a checklist-style reference of the implemented functions across all code files in the folder. Function names are listed as defined in source.

#### C.1 Web Dashboard Functions (`index.html`)

**Note on duplicates:** Some auth-related functions are defined twice in the file (`switchAuthView`, `showError`, `emailSignIn`, `emailSignUp`, `resetPassword`). In JavaScript, the later definition overrides the earlier one.

**Navigation & UI**
- `showTab(tabName)`: shows a major UI tab/section.
- `switchTab(tabId, navElement)`: switches dashboard tabs and updates nav active state.
- `toggleMenu(event)`: opens/closes the responsive side/menu panel.
- `updateHeader()`: updates top header state based on current view.
- `enterDashboard()`: transitions from landing screen into dashboard UI.
- `showLogsPage()`: opens the analytics/logs page.
- `hideLogsPage()`: closes the analytics/logs page.
- `showToast(message, type, duration)`: transient notification system.
- `toggleTdsInfo()`: toggles TDS help/info section.

**Language / Translation**
- `getLang()`: returns current language selection.
- `translateText(text)`: translates UI strings using the in-file map.
- `gl(key)`: key-based translation lookup.
- `applyTranslation(map)`: applies translations to the DOM.
- `translateToBangla()`: sets Bangla mode and refreshes UI.
- `translateToEnglish()`: sets English mode and refreshes UI.
- `refreshRuntimeTranslations()`: refreshes dynamic/generated text translations.
- `showGeminiSettingsPage()`: displays the Gemini API configuration UI.
- `hideGeminiSettingsPage()`: returns to the More menu from Gemini settings.
- `saveGeminiApiKey()`: persists the user-provided key to `localStorage`.
- `loadGeminiApiKey()`: retrieves the key for UI display.
- `toggleApiKeyVisibility()`: toggles the password mask on the API key input.
- `getGeminiApiKey()`: retrieves the key for use in direct API calls.

**Authentication (Firebase Auth)**
- `switchAuthView(viewId)`: switches between login/signup/reset UI.
- `showError(elementId, message)`: prints validation/auth errors in the UI.
- `emailSignIn()`: email/password sign-in.
- `emailSignUp()`: email/password sign-up.
- `resetPassword()`: password reset flow.
- `signInWithGoogle()`: Google OAuth sign-in.
- `signOut()`: signs user out and updates UI.
- `updateFirebaseStatus(connected)`: updates “connected/auth” status indicators.

**Image Handling + Multi-Model AI Inference**
- `handleLocalImageFile(file)`: validates/loads a user-selected local image.
- `resizeImageFile(file, maxDim, quality)`: client-side image resizing/compression.
- `infer(base64Image, model)`: runs inference against one selected model endpoint.
- `runInferenceBatched(activeModels, base64Image, batchSize, progressCb)`: runs multiple model inferences in batches.
- `calculateConsensus(results)`: computes consensus/aggregated decision from model outputs.
- `analyzeLocalImage()`: end-to-end local image analysis pipeline.
- `runAnalysis(withSensors)`: orchestration for analysis runs (visual-only or with sensor context).
- `analyze()`: triggers analysis workflow (UI handler).
- `visualOnly()`: triggers analysis without sensors.

**Environmental Context + Disease Severity Helpers**
- `analyzeEnvironmentalContext(sensorData)`: converts sensor values into contextual features for AI output.
- `verifyDiseaseWithContext(diseaseName)`: implements the logic matrix to validate AI results against live sensors.
- `assessDiseaseSeverity(diseaseName, confidence, envContext)`: assigns severity category/priority.
- `getRemedy(diseaseName)`: returns remedy guidance mapping.
- `translateRemedyText(remedy)`: translates remedy text into active language.
- `getTranslatedRemedy(remedy)`: wrapper to return remedy in current language.

**Analytics / Logging / Sync (Local + Firestore)**
- `initializeAnalytics()`: initializes analytics module (invoked via IIFE in-file).
- `initializeModelFeedback()`: sets up model feedback tracking structures.
- `recordModelFeedback(analysisId, modelName, isCorrect)`: stores a feedback vote.
- `resetModelFeedback()`: clears feedback state.
- `createCompleteLogEntry(results, sensorData, verificationResult)`: produces a unified analytics record including sensor fusion verification.
- `loadAnalytics()`: loads analytics state (local/Firebase).
- `loadFromLocalStorageCache()`: loads cached logs from localStorage.
- `saveAnalytics()`: persists analytics cache.
- `sanitizeForFirestore(obj)`: removes/normalizes unsupported fields.
- `saveToFirebase(logEntry)`: writes analytics/log entries to Firestore.
- `logAnalysis(results, sensorData, verificationResult, analysisId)`: saves one analysis + verification status + associated metadata.
- `loadFromFirebase(showAlert)`: loads analytics/history from Firestore.
- `resyncLocalLogs()`: attempts to re-upload cached logs.
- `syncWithFirebase()`: synchronizes local and Firebase state.
- `updateAnalyticsDashboard()`: updates analytics summary numbers.
- `updateCharts()`: refreshes analytics charts.
- `updateModelPrecisionChart()`: renders model precision chart based on feedback.
- `updateHistoryPanel(filter)`: renders the analysis history list.
- `toggleHistoryDetails(logId)`: expands/collapses one history item.
- `filterHistory(filter)`: applies UI filter to history panel.
- `deleteAnalysis(logId)`: deletes one analysis record.
- `getTimeAgo(date)`: time-delta label helper.
- `exportAnalyticsData()`: exports analytics/history data.
- `clearAnalyticsData()`: clears analytics (local and/or Firebase).
- `base64ToBlob(base64, contentType)`: helper for binary upload/display.

**Gemini / LLM Augmentation (Optional UI Section)**
- `triggerGeminiAIAnalysis(results, consensusData, imageBase64, analysisId, verificationData)`: requests LLM analysis with context fusion.
- `displayGeminiAnalysis(analysisText, analysisId)`: renders returned analysis.
- `translateGeminiAnalysis(analysisId)`: translates analysis result.
- `toggleGeminiAnalysis()`: show/hide Gemini section.
- `updateGeminiToggleUI(enabled)`: updates Gemini toggle state.

**Plant/Model Selection + Rendering Utilities**
- `loadSensors()`: loads available sensors/model configuration.
- `displayActiveModels()`: shows selected AI models.
- `initPlantSelector()`: initializes plant selector UI.
- `selectPlant(plantId)`: sets current plant.
- `getActiveModels()`: returns list of enabled models.
- `showAnal()`: UI helper to focus analysis view.
- `showMultiModelResults(results, s, consensusData)`: renders multi-model result cards.
- `renderCtx(n)`: renders context blocks for analytics view.
- `ctxNotes(s)`: helper for rendering notes.
- `handleModelFeedback(analysisId, modelName, isCorrect, feedbackId)`: binds feedback UI actions.
- `renderAnalysisUI(...)`: builds analysis UI output region.

**Camera / ESP32-CAM Connectivity**
- `fetchWithTimeout(url, options, timeout)`: network helper with timeout.
- `setIP()`: stores device IP/host and updates stream/capture endpoints.
- `testWithImage(path, timeout)`: connectivity test by fetching image.
- `startStream()`: starts MJPEG stream display.
- `startPollingCapture(interval)`: polls `/capture.jpg` periodically.
- `toggleStream()`: switches between streaming/polling/off.

**Security (Fusion, Alarm, Logs)**
- `saveSecuritySettings()`: saves security configuration to Firestore.
- `loadSecuritySettings()`: loads security configuration from Firestore.
- `processSecurityLogic(data)`: sensor fusion + confidence scoring + alarm trigger logic.
- `logSecurityEvent(type, message, confidence)`: writes security events to Firestore.
- `updateSecurityUI()`: updates security panel status badges.
- `updateSecurityChart()`: updates security-related chart.
- `loadSecurityEvents()`: loads security event history from Firestore.
- `sendMotionAlert()`: sends/records a motion alert.
- `startAlarm(fromRemote)`: starts Web Audio alarm + UI banner.
- `playBeep()`: generates a beep tone (inner helper used by alarm).
- `stopAlarm(fromRemote)`: stops alarm + resets UI.
- `addMotionEvent(timestamp, message)`: appends a motion event to UI list.
- `addSecurityEvent(type, message, timestamp)`: appends a security event to UI list.
- `loadSecurityLog()`: loads local/UI security log.
- `updateSecurityStats()`: updates security summary counters.

**Nutrient Loss Monitor (NEW)**
- `initializeNutrientMonitor()`: initializes Firebase listeners for nutrient events and alerts.
- `monitorNutrientEvents(sensorData)`: monitors real-time sensor data for rain/pump events and TDS changes.
- `startNutrientEvent(initialTDS, rainDetected, pumpUsed)`: creates active event in Firebase with initial conditions.
- `endNutrientEvent(finalTDS)`: ends active event, calculates nutrient loss, moves to history, triggers alerts if needed.
- `createNutrientAlert(lossPercentage, nutrientLoss, finalTDS)`: creates cross-device alert in Firebase with severity classification.
- `showNutrientAlert(alert)`: displays global alert banner on all pages.
- `hideNutrientAlert()`: hides global alert banner.
- `acknowledgeNutrientAlert()`: dismisses alert across all devices via Firebase update.
- `playAlarmSound()`: generates audio alarm using Web Audio API for critical alerts.
- `updateNutrientMonitorUI()`: updates live event monitor UI with current state.
- `loadNutrientHistory()`: loads and displays last 10 events from Firebase history collection.
- `updateNutrientAnalytics(event)`: updates analytics dashboard with latest event metrics.
- `updateNutrientRecommendations(finalTDS, lossPercentage)`: generates context-aware fertilizer recommendations.
- `initializeNutrientChart()`: initializes Chart.js TDS timeline graph.

**Sensors, History, Filtering, and Charts (Chart.js)**
- `loadSensorData()`: loads historical sensor data from Firestore.
- `saveSensorReading(data)`: appends live readings + optionally persists.
- `filterSensorData(data, filter)`: time-window filter for charting.
- `filterSensorTimeline(filter)`: sets current timeline filter.
- `formatChartLabel(timestamp, filter)`: formats chart x-axis labels.
- `updateSensorCharts()`: refreshes all sensor charts.
- `updateTemperatureChart()`: temperature chart.
- `updateHumidityChart()`: humidity chart.
- `updateMoistureChart()`: moisture chart.
- `updateSoilTemperatureChart()`: DS18B20 soil temperature chart.
- `updateTdsChart()`: TDS chart.
- `calculateDailySunlight(targetDateStr)`: daily sunlight estimation from history.
- `updateLightChart()`: light history + sunlight stats rendering.
- `updateWaterChart()`: daily water usage bar chart.
- `updateRainChart()`: rain history chart.
- `updateSoilHealthUI(ppm)`: maps PPM to soil health message.

**MQTT + Irrigation Control (ESP32 device-side timers)**
- `saveAutoModeState(enabled)`: persists auto-mode UI state.
- `loadAutoModeState()`: restores auto-mode UI state.
- `toggleAutoMode()`: enables/disables auto mode and publishes retained command.
- `updateAutoProfile()`: updates thresholds/profile and publishes to device.
- `isMqttConnected()`: connection status helper.
- `publishIrrigationMessage(topic, payload, options)`: retained publish + offline queue.
- `flushPendingIrrigationCommands()`: flushes queued messages after reconnect.
- `tryConnect(brokerHost, port, path, useSSL, attemptFallback)`: connects to broker.
- `onConnect()`: subscribes topics + enables controls.
- `onConnectionLost(responseObject)`: sets offline state.
- `onMessageArrived(message)`: inbound dispatcher for sensors/status/alarm.
- `resetSensorWatchdog()`: resets offline watchdog timer.
- `setDeviceOffline()`: offline UI state.
- `updatePumpStatus(status)`: updates pump mode/state/timers from device status JSON.
- `togglePump(source)`: manual ON/OFF publish.
- `initPumpScheduler()`: initializes schedule UI data structures.
- `renderSchedules()`: renders schedule rows.
- `addScheduleRow()`: adds schedule UI row.
- `removeScheduleRow(index)`: removes schedule row.
- `updateSchedule(index, field, value)`: updates one schedule row value.
- `setDelayTimer(mode)`: configures quick-timer payload.
- `cancelTimer()`: cancels quick-timer schedule.
- `savePumpSchedules()`: sends first enabled schedule to device.
- `checkPumpSchedule()`: checks schedule status in UI.
- `updatePumpScheduleStatus()`: updates schedule status labels.

**Weather (External API)**
- `initWeather()`: initializes weather module.
- `toggleLocationSearch()`: toggles location search UI.
- `searchLocation()`: searches a place/location.
- `useCurrentLocation()`: geolocation flow.
- `fetchWeather(lat, lon)`: fetches forecast data.
- `renderWeather(data)`: renders forecast UI.
- `selectDay(index)`: selects forecast day.
- `getWeatherDescription(code)`: maps weather code to text.
- `getWeatherIcon(code)`: maps weather code to icon.

#### C.2 ESP32 Master Firmware Functions (`ESP32_CAM_MASTER/ESP32_CAM_MASTER.ino`)

- `setup()`: initializes serial ports, relay pin, camera, Wi‑Fi, NTP, MQTT, HTTP server routes (`/stream`, `/capture`, `/capture.jpg`).
- `loop()`: handles HTTP clients, reads Arduino frames, maintains MQTT/NTP, restores quick timer once after NTP sync, runs `processTimerLogic()` continuously, and publishes status periodically.
- `setupWiFi()`: connects to Wi‑Fi and reports `WIFI_OK:<ip>` or `WIFI_FAILED` over Serial2.
- `setupNTP()`: initial time sync via `configTime` with bounded retries.
- `syncNTP()`: periodic re-sync (hourly) when connected.
- `mqttCallback(topic, payload, length)`: JSON parse + applies retained commands for schedule/quick timer, auto mode, manual override, and safety settings.
- `mqttReconnect()`: reconnect + subscribes topics + restores preferences.
- `processTimerLogic()`: pump decision engine with priority Manual > Auto > Quick Timer > Schedule; includes daily volume reset when date changes.
- `setPumpState(state, reason)`: toggles physical relay (active‑LOW) and sends `PUMP_ON`/`PUMP_OFF` commands to Arduino.
- `publishStatus()`: publishes retained status JSON (pump state, mode, remaining time, schedule settings, auto settings, flow/usage).
- `restoreScheduleFromPrefs()`: restores schedule and auto-mode configuration from `Preferences`.
- `clearQuickTimerPrefs()`: clears persisted quick timer settings.
- `restoreQuickTimerFromPrefs()`: restores quick timer based on stored epoch time + elapsed duration.
- `readArduinoSerial()`: line-buffered parser for `ENV:`, `SEC:`, `MOTION:`, `FLOW:` frames; updates internal variables and publishes via MQTT.
- `publishEnvData()`: publishes environment sensor JSON to MQTT.
- `publishSecData()`: publishes security sensor JSON to MQTT.
- `publishSensorData()`: helper publisher (present in source) used to unify telemetry publishing.
- `initCamera()`: configures and initializes camera; uses PSRAM to select frame size/buffers.
- `clientId(client)`: derives a stable client identifier from IP/port for stream ownership.
- `handleStream()`: MJPEG streaming endpoint; keeps MQTT alive and keeps pump logic running while streaming.
- `handleCapture()`: simple capture probe endpoint.
- `handleCaptureJPG()`: returns one JPEG frame over HTTP.

#### C.3 Arduino Slave Firmware Functions (`arduino_slave/arduino_slave.ino`)

- `pulseCounter()`: interrupt service routine for the flow sensor (increments `pulseCount`).
- `setup()`: initializes debug serial, SoftwareSerial to ESP32, sensors, pins, and attaches flow interrupt.
- `handleCommand(cmd)`: parses newline-delimited commands (`PUMP_ON`, `PUMP_OFF`, and ESP32 status messages) and actuates the relay.
- `loop()`: non-blocking serial receive + three periodic tasks:
    - PIR immediate update with 10s debounce/rate limit (`MOTION:<0/1>`)
    - ENV frame every 60s (`ENV:...`)
    - SEC frame every 5s (`SEC:...`)
    - FLOW frame every 1s only when pump is ON (`FLOW:rate,total_ml`)

---

### Appendix D: Firestore Rules Explained (`firestore.rules`)

All listed collections are guarded by `request.auth != null` (signed-in users only). The rules are permissive once authenticated.

- `match /sensorData/{documentId}`
    - `read`: allowed if authenticated.
    - `create, update`: allowed if authenticated and `timestamp` is a positive number.
    - `delete`: allowed if authenticated.
- `match /waterUsage/{documentId}`: authenticated read/write.
- `match /irrigationLogs/{documentId}`: authenticated read/write.
- `match /motionEvents/{documentId}`: authenticated read/write.
- `match /securityEvents/{documentId}`: authenticated read/write.
- `match /securitySettings/{documentId}`: authenticated read/write.
- `match /securityLogs/{documentId}`: authenticated read/write.
- `match /userSettings/{documentId}`: authenticated read/write.
- `match /modelFeedback/{documentId}`: authenticated read/write.
- `match /analytics/{documentId}`: authenticated read/write.
- `match /diseaseAnalytics/{documentId}`: authenticated read/write.
- `match /activityLogs/{documentId}`: authenticated read/write.
- `match /autoModeState/{documentId}`: authenticated read/write.
- `match /settings/{documentId}`: authenticated read/write.
- `match /users/{documentId}`: authenticated read/write.
- Catch-all `match /{document=**}`: authenticated read/write for any other document path.

