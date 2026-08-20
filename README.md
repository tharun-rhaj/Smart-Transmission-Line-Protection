# ⚡ Smart Transmission Line Protection System

An IoT-based transmission line monitoring and protection system built using **ESP32**, designed to detect abnormal line conditions and automatically isolate the protected line through a remotely controlled breaker unit.

The system consists of two ESP32 units:

* **ESP32 #1 — Protection Unit:** Monitors current, vibration, tilt and GPS data, performs fault detection, hosts a live monitoring dashboard, and transmits fault information using ESP-NOW.
* **ESP32 #2 — Breaker Unit:** Receives the fault information through ESP-NOW and controls a relay to isolate the line.

---

## 📌 Project Overview

Transmission lines can be affected by conditions such as conductor disconnection, excessive current, pole tilting and abnormal mechanical vibration.

This prototype provides a multi-sensor approach for detecting these conditions and automatically responding to faults.

### System Flow

```text
Sensors
   │
   ▼
ESP32 Protection Unit
   │
   ├── Fault Detection
   ├── GPS Location
   ├── Web Dashboard
   └── ESP-NOW Communication
              │
              ▼
       ESP32 Breaker Unit
              │
              ▼
            Relay
              │
              ▼
       Line Isolation
```

---

# 🔧 Hardware Components

| Component    | Purpose                               |
| ------------ | ------------------------------------- |
| ESP32 × 2    | Protection and breaker control        |
| ACS712       | Line current measurement              |
| SW420        | Vibration detection                   |
| SW520D       | Pole tilt detection                   |
| NEO-8M GPS   | Fault location tracking               |
| Relay Module | Line isolation                        |
| DC Load      | Transmission-line prototype test load |

---

# 🛡️ Protection Unit

The Protection Unit continuously monitors the transmission-line model and evaluates the connected sensors.

### Features

* ACS712 current measurement
* Automatic ACS712 zero-point calibration at startup
* SW420 vibration monitoring
* SW520D tilt monitoring
* NEO-8M GPS location tracking
* Multi-fault classification
* Local Wi-Fi Access Point
* Real-time web dashboard
* AJAX-based live telemetry
* Fault/event history
* Google Maps location integration
* ESP-NOW communication with the Breaker Unit

The Protection Unit checks the sensors at regular intervals and sends the detected fault status to the Breaker Unit.

---

# ⚡ Current Monitoring

The ACS712 current sensor is calibrated during startup with the load turned OFF.

The prototype was tested with approximately:

```text
Load connected    → ~16 A
Load disconnected → ~0 A
```

This allows the system to identify a significant drop in current as a possible line-disconnection condition.

## Current Threshold Configuration

The current limits are configurable according to the user's own load and sensor calibration.

The values currently included in this repository are the values used for the prototype:

```text
LOW_CURRENT_THRESHOLD  = 16.40 A
HIGH_CURRENT_THRESHOLD = 18.01 A
```

### Interpretation

| Condition                    | Interpretation |
| ---------------------------- | -------------- |
| Below low-current threshold  | LINE SNAP      |
| Between configured limits    | NORMAL         |
| Above high-current threshold | OVERCURRENT    |

> **Important:** The threshold values are not universal. When adapting this project to a different load or transmission-line model, calibrate the ACS712 and configure the minimum and maximum current limits accordingly.

---

# 🚨 Fault Detection

The Protection Unit supports the following fault classifications:

```text
0 → NO FAULT
1 → LINE SNAP
2 → TILT FAULT
3 → VIBRATION FAULT
4 → OVERCURRENT
5 → MULTIPLE FAULTS
```

If multiple fault conditions occur simultaneously, the system reports:

```text
MULTIPLE FAULTS
```

with the active fault conditions included in the status message.

---

# 📡 ESP-NOW Communication

The two ESP32 units communicate using **ESP-NOW**.

The Protection Unit sends a compact message containing:

```text
fault → Whether a fault is active
type  → Fault classification
```

### Communication Flow

```text
┌─────────────────────┐
│ ESP32 Protection    │
│      Unit           │
│                     │
│ Sensor Monitoring   │
│ Fault Detection     │
└──────────┬──────────┘
           │
           │ ESP-NOW
           ▼
┌─────────────────────┐
│ ESP32 Breaker Unit  │
│                     │
│ Fault Reception     │
│ Relay Control       │
└──────────┬──────────┘
           │
           ▼
        RELAY
           │
           ▼
    LINE ISOLATION
```

---

# 🔌 Breaker Unit

The Breaker Unit receives the fault message from the Protection Unit.

When a fault is detected, the relay is switched to the tripped state.

The relay used in the prototype is **active-LOW**:

```text
LOW  → Relay ON  → Breaker CLOSED
HIGH → Relay OFF → Breaker TRIPPED
```

The Breaker Unit prevents repeated tripping and waits for manual inspection/reset after a fault.

### Breaker Unit behavior

```text
Fault received
      ↓
Fault identified
      ↓
Breaker trips
      ↓
Relay switches OFF
      ↓
Line isolated
      ↓
Manual inspection/reset
```

---

# 📍 Pin Configuration

## ESP32 Protection Unit

| Component              | ESP32 Pin |
| ---------------------- | --------: |
| ACS712 Current Sensor  |   GPIO 34 |
| SW420 Vibration Sensor |   GPIO 26 |
| SW520D Tilt Sensor     |   GPIO 27 |
| NEO-8M GPS RX          |   GPIO 16 |
| NEO-8M GPS TX          |   GPIO 17 |

The GPS module uses **HardwareSerial 2**.

## ESP32 Breaker Unit

| Component    | ESP32 Pin |
| ------------ | --------: |
| Relay Module |   GPIO 26 |

---

# 🌐 Live Web Dashboard

The Protection Unit hosts a local web dashboard through its Wi-Fi Access Point.

The dashboard provides real-time information about:

* Current load
* Pole tilt
* Wire vibration
* GPS status and coordinates
* Overall system status
* Fault classification
* Fault history
* Node information
* ESP-NOW status
* Google Maps location

The dashboard uses AJAX telemetry updates, allowing sensor information to be updated without reloading the complete webpage.

### Dashboard Preview

Add your cropped dashboard image to the repository, for example:

```text
Images/dashboard_fault_detection.jpg
```

Then display it in this section using:

```markdown
![Live Dashboard](Images/dashboard_fault_detection.jpg)
```

---

# 📍 GPS Location

The NEO-8M GPS module provides latitude and longitude information for the monitored node.

When a valid GPS fix is available, the dashboard displays:

```text
Latitude
Longitude
```

and provides a Google Maps location link.

When a GPS fix is unavailable, the dashboard displays:

```text
NO FIX
```

---

# 📊 Dashboard Monitoring

The dashboard provides a SCADA-style interface containing:

```text
┌──────────────────────────────────────────────┐
│              LINE SHIELD                     │
│     Smart Transmission Line Protection       │
├──────────────────────────────────────────────┤
│              SYSTEM STATUS                   │
├────────────┬────────────┬──────────┬─────────┤
│ Current    │ Pole Tilt  │Vibration │   GPS   │
│            │            │          │         │
├────────────┴────────────┴──────────┴─────────┤
│ Diagnostics Matrix │ Event Log │ Node Data   │
└──────────────────────────────────────────────┘
```

The system also maintains a live fault history for detected abnormal conditions.

---

# 💻 Software Requirements

### Development Environment

* Arduino IDE
* ESP32 Board Package

### Protection Unit Libraries

```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <esp_now.h>
```

### Breaker Unit Libraries

```cpp
#include <WiFi.h>
#include <esp_now.h>
```

Install the required ESP32 board support and the **TinyGPS++** library before compiling the Protection Unit.

---

# 🚀 Installation & Setup

## 1. Clone or download the repository

Download the repository to your computer.

## 2. Open the Protection Unit code

Open:

```text
ESP32_Protection_Unit/
└── protection_unit.ino
```

## 3. Configure the Wi-Fi Access Point

Configure your own Wi-Fi credentials:

```cpp
const char* ssid = "LineShield_AP";
const char* password = "YOUR_AP_PASSWORD";
```

## 4. Configure the ESP-NOW receiver

Set the receiver MAC address to the MAC address of your Breaker Unit ESP32.

## 5. Upload the Protection Unit

Connect the first ESP32 and upload:

```text
ESP32_Protection_Unit/protection_unit.ino
```

> Keep the current-sensor load OFF during startup so that the ACS712 zero-current point can be calibrated.

## 6. Upload the Breaker Unit

Connect the second ESP32 and upload:

```text
ESP32_Breaker_Unit/breaker_unit.ino
```

## 7. Start the system

Power both ESP32 units and connect to the Protection Unit's Wi-Fi Access Point.

The Protection Unit provides the local dashboard for monitoring the system.

---

# 🧪 Testing

The prototype was tested using a DC load connected to the monitored line.

### Normal condition

```text
Load connected
      ↓
Current ≈ 16 A
      ↓
System monitors the line
```

### Line disconnection

```text
Load disconnected
      ↓
Current drops toward ≈ 0 A
      ↓
LINE SNAP detected
      ↓
Fault transmitted using ESP-NOW
      ↓
Breaker Unit receives fault
      ↓
Relay trips
```

Other test conditions include:

* Pole tilt
* Vibration
* Overcurrent
* Multiple simultaneous faults

---

# 📁 Repository Structure

```text
Smart-Transmission-Line-Protection/
│
├── ESP32_Protection_Unit/
│   └── protection_unit.ino
│
├── ESP32_Breaker_Unit/
│   └── breaker_unit.ino
│
├── Images/
│   └── dashboard_fault_detection.jpg
│
└── README.md
```

---

# 📈 Project Status

### Implemented

* [x] ACS712 current monitoring
* [x] Automatic current-sensor calibration
* [x] Line-snap detection
* [x] Overcurrent detection
* [x] Pole-tilt detection
* [x] Vibration detection
* [x] Multiple-fault detection
* [x] GPS integration
* [x] ESP-NOW communication
* [x] Relay-based breaker control
* [x] Local Wi-Fi Access Point
* [x] Real-time web dashboard
* [x] AJAX live telemetry
* [x] Fault history/event log
* [x] Google Maps integration

---

# 🚀 Future Improvements

Possible future improvements include:

* Remote breaker reset
* Cloud-based monitoring
* Long-range communication
* Improved fault-location analysis
* Multiple transmission-line nodes
* Centralized monitoring of multiple poles
* Secure ESP-NOW communication
* Improved hardware packaging and enclosure
* More advanced current-sensor calibration
* Remote monitoring from outside the local network

---

# 👨‍💻 Project Information

**Project:** Smart Transmission Line Protection System

**Platform:** ESP32

**Programming:** C/C++ with Arduino Framework

**Communication:** ESP-NOW

**Monitoring:** Local Web Dashboard

**Sensors:** ACS712, SW420, SW520D, NEO-8M GPS

**Application:** Transmission-line fault monitoring and automatic protection

---

## ⭐ Project Highlights

This project combines **embedded systems, IoT, wireless communication, sensor fusion, fault detection, GPS tracking and web-based monitoring** into a single transmission-line protection prototype.

The two-node architecture separates **fault detection** from **breaker control**, allowing the protection and isolation functions to operate on independent ESP32 controllers.
