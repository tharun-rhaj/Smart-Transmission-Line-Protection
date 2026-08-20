/*****************************************************
 SMART TRANSMISSION LINE PROTECTION SYSTEM
 ESP32 #1 - Protection Unit
 (COMBINED & OPTIMIZED VERSION)

 FEATURES
 --------------------------------------
 ✔ ACS712 Current Sensor (calibrated: 66mV/A, auto zero-point)
 ✔ SW420 Vibration Sensor
 ✔ SW520D Tilt Sensor
 ✔ NEO-8M GPS (HardwareSerial 2, pins 16 & 17)
 ✔ WiFi Access Point (AP mode with STA coexistence)
 ✔ Local HTML/CSS SCADA Web Dashboard
 ✔ AJAX Live Telemetry Sync (no page reloads)
 ✔ Multi-Fault Detection:
     NO FAULT / LINE SNAP / TILT / VIBRATION / OVERCURRENT / MULTIPLE
 ✔ Interactive Event Log & Fault History
 ✔ Google Maps Location Integration
 ✔ ESP-NOW Ready (Sends status to Breaker ESP32)
*****************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <esp_now.h>

//-------------------------
// Pin Definitions
//-------------------------
#define CURRENT_PIN    34
#define VIBRATION_PIN  26
#define TILT_PIN       27
#define GPS_RX_PIN     16
#define GPS_TX_PIN     17

//-------------------------
// GPS UART (Hardware Serial 2)
//-------------------------
HardwareSerial GPSserial(2);
TinyGPSPlus gps;

//-------------------------
// WiFi Settings
//-------------------------
const char* ssid = "LineShield_AP";
const char* password = "Your_AP_Password";

WebServer server(80);

String POLE_ID = "P-01";

//-------------------------
// Current Calibration & Limits
//-------------------------
float current = 0;
float zeroVoltage = 2.1325; // ACS712 zero-current voltage, calibrated at boot
float sensitivity = 0.066;  // 66 mV/A for ACS712-30A module

const float LOW_CURRENT_THRESHOLD  = 16.40A; // Below this = LINE SNAP (load disconnected)
const float HIGH_CURRENT_THRESHOLD = 18.01; // Above this = OVERCURRENT (overload fault)

//-------------------------
// Fault & Status Variables
//-------------------------
bool vibrationFault = false;
bool tiltFault = false;
bool currentFault = false;
bool gpsValid = false;

// 0=none, 1=line snap, 2=tilt, 3=vibration, 4=overcurrent, 5=multiple
int faultType = 0; 
String faultStatus = "NO FAULT";
String faultHistory = "";

float latitude = 0;
float longitude = 0;

unsigned long previousMillis = 0;
const long telemetryInterval = 500; // telemetry updates and ESP-NOW send interval (ms)

//-------------------------
// ESP-NOW Communications
//-------------------------
typedef struct
{
  bool fault;
  int type;
} message;

message outgoingData;

// Breaker ESP32 Destination MAC Address (Edit this to match target hardware)
uint8_t receiverMAC[] = {0x04, 0xB2, 0x47, 0x96, 0xCC, 0x88};

//-------------------------
// HTML Dashboard (Served at /)
//-------------------------
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>LINE SHIELD - Smart Transmission Line Protection System</title>
  <style>
    /* Reset and General Layout */
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }

    body {
      background-color: #0F172A;
      color: #F8FAFC;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
      line-height: 1.5;
      padding-bottom: 3rem;
    }

    .dashboard-wrapper {
      max-width: 1200px;
      margin: 0 auto;
      padding: 0 1.5rem;
      display: flex;
      flex-direction: column;
      gap: 1.5rem;
    }

    /* Top Header */
    header {
      background-color: #1E293B;
      border-bottom: 1px solid #334155;
      padding: 1rem 2rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 1rem;
      margin-bottom: 1.5rem;
    }

    .brand-group {
      display: flex;
      align-items: center;
      gap: 0.75rem;
    }

    .logo-container {
      color: #38BDF8;
      display: flex;
      align-items: center;
      justify-content: center;
    }

    .logo-container svg {
      width: 28px;
      height: 28px;
      filter: drop-shadow(0 0 8px rgba(56, 189, 248, 0.4));
    }

    .title-area h1 {
      font-size: 1.25rem;
      font-weight: 700;
      color: #FFFFFF;
      letter-spacing: -0.02em;
    }

    .title-area p {
      font-size: 0.75rem;
      color: #94A3B8;
      font-weight: 500;
    }

    .header-status-meta {
      display: flex;
      align-items: center;
      gap: 1.5rem;
    }

    .online-indicator {
      display: inline-flex;
      align-items: center;
      gap: 0.5rem;
      font-size: 0.75rem;
      font-weight: 700;
      color: #94A3B8;
      letter-spacing: 0.05em;
    }

    .status-dot {
      width: 8px;
      height: 8px;
      background-color: #10B981;
      border-radius: 50%;
      display: inline-block;
      box-shadow: 0 0 8px #10B981;
      animation: pulse 2s infinite;
    }

    @keyframes pulse {
      0% { opacity: 0.4; }
      50% { opacity: 1; }
      100% { opacity: 0.4; }
    }

    .pole-id-badge {
      font-size: 0.75rem;
      font-weight: 700;
      background-color: #334155;
      color: #F8FAFC;
      padding: 0.25rem 0.75rem;
      border-radius: 4px;
      border: 1px solid #475569;
    }

    /* Page Title Section */
    .page-title-section {
      display: flex;
      justify-content: space-between;
      align-items: flex-end;
      border-bottom: 1px solid #334155;
      padding-bottom: 1rem;
      margin-top: 0.5rem;
    }

    .page-title-area h2 {
      font-size: 1.5rem;
      font-weight: 700;
      color: #FFFFFF;
    }

    .page-title-area p {
      font-size: 0.875rem;
      color: #94A3B8;
    }

    .live-meta {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      font-size: 0.8rem;
      font-weight: 600;
      color: #94A3B8;
    }

    .live-dot {
      width: 6px;
      height: 6px;
      background-color: #38BDF8;
      border-radius: 50%;
      box-shadow: 0 0 6px #38BDF8;
    }

    /* System Status Banner */
    .status-banner {
      border-radius: 8px;
      padding: 1.25rem 1.5rem;
      border-left: 4px solid;
      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
      transition: all 0.3s ease;
    }

    .status-banner-normal {
      background-color: rgba(6, 78, 59, 0.2);
      border-color: #10B981;
      color: #A7F3D0;
    }

    .status-banner-fault {
      background-color: rgba(127, 29, 29, 0.2);
      border-color: #EF4444;
      color: #FCA5A5;
    }

    .status-banner-title {
      font-size: 1.1rem;
      font-weight: 700;
      margin-bottom: 0.25rem;
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }

    .status-banner-desc {
      font-size: 0.875rem;
      opacity: 0.9;
    }

    /* Sensor Grid Layout */
    .sensor-grid {
      display: grid;
      grid-template-columns: repeat(4, 1fr);
      gap: 1.25rem;
    }

    @media (max-width: 1024px) {
      .sensor-grid {
        grid-template-columns: repeat(2, 1fr);
      }
    }

    @media (max-width: 640px) {
      .sensor-grid {
        grid-template-columns: 1fr;
      }
    }

    /* Card Styling */
    .card {
      background-color: #1E293B;
      border: 1px solid #334155;
      border-radius: 8px;
      padding: 1.25rem;
      display: flex;
      flex-direction: column;
      justify-content: space-between;
      min-height: 165px;
      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
      transition: all 0.2s ease;
    }

    .card:hover {
      transform: translateY(-2px);
      box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.3);
      border-color: #475569;
    }

    .card-top {
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      margin-bottom: 0.5rem;
    }

    .card-label {
      font-size: 0.75rem;
      font-weight: 700;
      color: #94A3B8;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }

    .card-icon {
      color: #94A3B8;
      display: flex;
      align-items: center;
      justify-content: center;
    }

    .card-icon svg {
      width: 20px;
      height: 20px;
    }

    .card-middle {
      margin-bottom: auto;
      padding: 0.25rem 0;
    }

    .card-value {
      font-size: 1.8rem;
      font-weight: 700;
      color: #FFFFFF;
      display: flex;
      align-items: baseline;
      gap: 0.15rem;
    }

    .card-unit {
      font-size: 1rem;
      color: #94A3B8;
      font-weight: 600;
    }

    .card-threshold {
      font-size: 0.75rem;
      color: #94A3B8;
      margin-top: 0.25rem;
    }

    .card-bottom {
      margin-top: 1rem;
    }

    /* Custom Progress Bar for Current Card */
    .bar-container {
      width: 100%;
      height: 6px;
      background-color: #334155;
      border-radius: 3px;
      overflow: hidden;
      margin-bottom: 0.5rem;
    }

    .bar-fill {
      height: 100%;
      width: 0%;
      background-color: #3B82F6;
      border-radius: 3px;
      transition: width 0.4s ease, background-color 0.2s ease;
    }

    /* Dynamic Card Border/Styles on Warnings */
    .card-warning-active {
      border-color: #EF4444 !important;
      box-shadow: 0 0 10px rgba(239, 68, 68, 0.2);
    }

    .card-warning-active .card-icon {
      color: #EF4444;
    }

    /* Flat Status Badges */
    .badge {
      display: inline-flex;
      align-items: center;
      font-size: 0.72rem;
      font-weight: 700;
      padding: 0.2rem 0.6rem;
      border-radius: 4px;
      border: 1px solid transparent;
      text-transform: uppercase;
      letter-spacing: 0.02em;
    }

    .badge-normal {
      background-color: rgba(6, 78, 59, 0.4);
      color: #34D399;
      border-color: rgba(52, 211, 153, 0.3);
    }

    .badge-fault {
      background-color: rgba(127, 29, 29, 0.4);
      color: #FCA5A5;
      border-color: rgba(252, 165, 165, 0.3);
    }

    .badge-warning {
      background-color: rgba(120, 53, 15, 0.4);
      color: #FCD34D;
      border-color: rgba(252, 211, 77, 0.3);
    }

    .badge-info {
      background-color: rgba(30, 58, 138, 0.4);
      color: #93C5FD;
      border-color: rgba(147, 197, 253, 0.3);
    }

    /* Details Grid: 3 Column Layout */
    .details-grid {
      display: grid;
      grid-template-columns: 1.2fr 1fr 1fr;
      gap: 1.5rem;
      margin-top: 0.5rem;
    }

    @media (max-width: 900px) {
      .details-grid {
        grid-template-columns: 1fr;
      }
    }

    .detail-card {
      background-color: #1E293B;
      border: 1px solid #334155;
      border-radius: 8px;
      padding: 1.5rem;
      box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
      display: flex;
      flex-direction: column;
    }

    .detail-card-title {
      font-size: 1rem;
      font-weight: 700;
      color: #FFFFFF;
      margin-bottom: 1rem;
      padding-bottom: 0.5rem;
      border-bottom: 1px solid #334155;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }

    /* Tables */
    .scada-table {
      width: 100%;
      border-collapse: collapse;
    }

    .scada-table th {
      text-align: left;
      font-size: 0.75rem;
      font-weight: 700;
      text-transform: uppercase;
      color: #94A3B8;
      padding-bottom: 0.75rem;
      border-bottom: 2px solid #334155;
    }

    .scada-table td {
      padding: 0.75rem 0;
      font-size: 0.875rem;
      border-bottom: 1px solid #334155;
      color: #CBD5E1;
    }

    .scada-table tr:last-child td {
      border-bottom: none;
    }

    .scada-table td.param-col {
      font-weight: 600;
      color: #FFFFFF;
    }

    .scada-table td.status-col {
      text-align: right;
    }

    /* Key-Value Lists */
    .info-list {
      display: flex;
      flex-direction: column;
      gap: 0.75rem;
    }

    .info-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      font-size: 0.875rem;
      border-bottom: 1px solid #334155;
      padding-bottom: 0.5rem;
    }

    .info-item:last-child {
      border-bottom: none;
      padding-bottom: 0;
    }

    .info-key {
      color: #94A3B8;
      font-weight: 500;
    }

    .info-value {
      color: #FFFFFF;
      font-weight: 700;
    }

    /* Terminal Log Container */
    .log-container {
      background-color: #0F172A;
      border: 1px solid #334155;
      border-radius: 6px;
      padding: 10px;
      height: 180px;
      overflow-y: auto;
      text-align: left;
      font-family: Consolas, Monaco, "Andale Mono", monospace;
      font-size: 0.75rem;
      color: #38BDF8;
      line-height: 1.4;
    }

    /* Scrollbar Styling */
    .log-container::-webkit-scrollbar {
      width: 6px;
    }
    .log-container::-webkit-scrollbar-track {
      background: #0F172A;
    }
    .log-container::-webkit-scrollbar-thumb {
      background: #334155;
      border-radius: 3px;
    }
    .log-container::-webkit-scrollbar-thumb:hover {
      background: #475569;
    }

    /* Styled Button */
    .maps-btn {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 0.5rem;
      width: 100%;
      padding: 0.6rem;
      font-size: 0.82rem;
      font-weight: 700;
      border: none;
      border-radius: 6px;
      background-color: #2563EB;
      color: white;
      cursor: pointer;
      text-decoration: none;
      transition: background-color 0.2s, transform 0.1s;
      margin-top: 1rem;
    }

    .maps-btn:hover {
      background-color: #1D4ED8;
    }

    .maps-btn:active {
      transform: scale(0.98);
    }

    /* Footer */
    footer {
      display: flex;
      justify-content: space-between;
      align-items: center;
      font-size: 0.75rem;
      color: #64748B;
      margin-top: 2rem;
      padding-top: 1rem;
      border-top: 1px solid #334155;
    }
  </style>
</head>
<body>

  <!-- Top Header -->
  <header>
    <div class="brand-group">
      <div class="logo-container">
        <!-- Shield SVG Icon -->
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
          <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
        </svg>
      </div>
      <div class="title-area">
        <h1>LINE SHIELD</h1>
        <p>Smart Transmission Line Protection System</p>
      </div>
    </div>
    <div class="header-status-meta">
      <div class="online-indicator">
        <span class="status-dot"></span>
        <span>SYSTEM AP ACTIVE</span>
      </div>
      <div class="pole-id-badge" id="header-pole-id">
        POLE ID: P-01
      </div>
    </div>
  </header>

  <div class="dashboard-wrapper">
    <!-- Page Title Section -->
    <div class="page-title-section">
      <div class="page-title-area">
        <h2>Distribution Line Telemetry</h2>
        <p>Real-time telemetry and diagnostic metrics from active node</p>
      </div>
      <div class="live-meta">
        <span class="live-dot"></span>
        <span>Live Telemetry</span>
      </div>
    </div>

    <!-- Main System Status Card -->
    <div id="system-status-banner" class="status-banner status-banner-normal">
      <div id="status-title" class="status-banner-title">
        SYSTEM NORMAL
      </div>
      <div id="status-desc" class="status-banner-desc">
        No abnormal conditions detected. Continuous monitoring active.
      </div>
    </div>

    <!-- Sensor Grid -->
    <div class="sensor-grid">
      <!-- Card 1: Current Load -->
      <div id="card-current" class="card">
        <div class="card-top">
          <span class="card-label">Current Load</span>
          <span class="card-icon">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/>
            </svg>
          </span>
        </div>
        <div class="card-middle">
          <div class="card-value">
            <span id="current-display-val">0.00</span>
            <span class="card-unit">A</span>
          </div>
          <div class="card-threshold">Limits: 16.40 A - 18.01 A</div>
        </div>
        <div class="card-bottom">
          <div class="bar-container">
            <div id="current-bar-fill" class="bar-fill"></div>
          </div>
          <span id="current-status-badge" class="badge badge-normal">OFF</span>
        </div>
      </div>

      <!-- Card 2: Pole Tilt -->
      <div id="card-tilt" class="card">
        <div class="card-top">
          <span class="card-label">Pole Tilt</span>
          <span class="card-icon">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <line x1="3" y1="21" x2="21" y2="21" />
              <line id="pole-line" x1="12" y1="21" x2="12" y2="3" stroke-width="2.5" />
              <line x1="12" y1="21" x2="12" y2="3" stroke-dasharray="2 2" stroke-opacity="0.3" />
            </svg>
          </span>
        </div>
        <div class="card-middle">
          <div class="card-value" style="font-size: 1.5rem;" id="tilt-text">
            NORMAL
          </div>
        </div>
        <div class="card-bottom">
          <span id="tilt-status-badge" class="badge badge-normal">NORMAL</span>
        </div>
      </div>

      <!-- Card 3: Vibration -->
      <div id="card-vibration" class="card">
        <div class="card-top">
          <span class="card-label">Wire Vibration</span>
          <span class="card-icon">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M2 10s3-5 5-5 5 10 5 10 5-5 5-5 3 5 5 5"/>
            </svg>
          </span>
        </div>
        <div class="card-middle">
          <div class="card-value" style="font-size: 1.5rem;" id="vibration-text">
            NORMAL
          </div>
        </div>
        <div class="card-bottom">
          <span id="vibration-status-badge" class="badge badge-normal">NORMAL</span>
        </div>
      </div>

      <!-- Card 4: GPS Location -->
      <div id="card-gps" class="card">
        <div class="card-top">
          <span class="card-label">GPS coordinates</span>
          <span class="card-icon">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0 1 18 0z" />
              <circle cx="12" cy="10" r="3" />
            </svg>
          </span>
        </div>
        <div class="card-middle">
          <div class="card-value" style="font-size: 1.25rem; font-weight: 700;" id="gps-display-status">
            CONNECTED
          </div>
          <div style="font-size: 0.75rem; color: #94A3B8; font-family: monospace; margin-top: 0.25rem;">
            LAT: <span id="gps-lat">0.000000</span><br>
            LON: <span id="gps-lon">0.000000</span>
          </div>
        </div>
        <div class="card-bottom">
          <span id="gps-status-badge" class="badge badge-normal">CONNECTED</span>
        </div>
      </div>
    </div>

    <!-- Bottom Row: Details Section -->
    <div class="details-grid">
      <!-- Fault Analysis Card -->
      <div class="detail-card">
        <div class="detail-card-title">Diagnostics Matrix</div>
        <table class="scada-table">
          <thead>
            <tr>
              <th>Conductor Parameter</th>
              <th style="text-align: right;">Status</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td class="param-col">Conductor Current</td>
              <td class="status-col" id="table-row-current"><span class="badge badge-normal">NORMAL</span></td>
            </tr>
            <tr>
              <td class="param-col">Suspension Pole Tilt</td>
              <td class="status-col" id="table-row-tilt"><span class="badge badge-normal">NORMAL</span></td>
            </tr>
            <tr>
              <td class="param-col">Conductor Vibration</td>
              <td class="status-col" id="table-row-vibration"><span class="badge badge-normal">NORMAL</span></td>
            </tr>
            <tr>
              <td class="param-col">Combined Diagnostics</td>
              <td class="status-col" id="table-row-overall"><span class="badge badge-normal">SYSTEM NORMAL</span></td>
            </tr>
          </tbody>
        </table>
      </div>

      <!-- Event Log (Fault History) -->
      <div class="detail-card">
        <div class="detail-card-title">Security Event Log</div>
        <div class="log-container" id="history-log">
          System Initialized...
        </div>
      </div>

      <!-- Node Properties -->
      <div class="detail-card">
        <div class="detail-card-title">Node Metadata</div>
        <div class="info-list">
          <div class="info-item">
            <span class="info-key">Hardware ID</span>
            <span class="info-value" id="info-pole-id">P-01</span>
          </div>
          <div class="info-item">
            <span class="info-key">ESP-NOW Link</span>
            <span class="info-value" style="color: #34D399;">Ready</span>
          </div>
          <div class="info-item">
            <span class="info-key">GPS Signal</span>
            <span class="info-value" id="info-gps-status">CONNECTED</span>
          </div>
          <div class="info-item">
            <span class="info-key">Last Check</span>
            <span class="info-value" id="info-last-update">--:--:--</span>
          </div>
          <div class="info-item">
            <span class="info-key">Fault Code</span>
            <span class="info-value" id="info-fault-type" style="color: #34D399;">0</span>
          </div>
        </div>
        <a id="maps-btn" class="maps-btn" target="_blank" href="#">
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
            <polygon points="3 6 9 3 15 6 21 3 21 18 15 21 9 18 3 21"/>
            <line x1="9" y1="3" x2="9" y2="18"/>
            <line x1="15" y1="6" x2="15" y2="21"/>
          </svg>
          Google Maps Location
        </a>
      </div>
    </div>

    <!-- Footer metadata -->
    <footer>
      <span>Line Shield SCADA Node v3.0 (Combined)</span>
      <span>Node clock: <span id="clock-display">--:--:--</span></span>
    </footer>
  </div>

  <!-- JavaScript Telemetry Logic -->
  <script>
    function updateDashboard(data) {
      const now = new Date();
      const timeStr = now.toLocaleTimeString();
      document.getElementById('clock-display').textContent = timeStr;
      document.getElementById('info-last-update').textContent = timeStr;

      // Update Pole IDs
      document.getElementById('header-pole-id').textContent = "POLE ID: " + data.poleId;
      document.getElementById('info-pole-id').textContent = data.poleId;

      // Status components
      const banner = document.getElementById('system-status-banner');
      const statusTitle = document.getElementById('status-title');
      const statusDesc = document.getElementById('status-desc');

      // Cards
      const cardCurrent = document.getElementById('card-current');
      const cardTilt = document.getElementById('card-tilt');
      const cardVibration = document.getElementById('card-vibration');
      const cardGps = document.getElementById('card-gps');

      // Table Status elements
      const rowCurrent = document.getElementById('table-row-current');
      const rowTilt = document.getElementById('table-row-tilt');
      const rowVibration = document.getElementById('table-row-vibration');
      const rowOverall = document.getElementById('table-row-overall');

      // Badge / Values
      const currentValSpan = document.getElementById('current-display-val');
      const currentBar = document.getElementById('current-bar-fill');
      const currentBadge = document.getElementById('current-status-badge');

      const tiltText = document.getElementById('tilt-text');
      const tiltBadge = document.getElementById('tilt-status-badge');
      const poleLine = document.getElementById('pole-line');

      const vibrationText = document.getElementById('vibration-text');
      const vibrationBadge = document.getElementById('vibration-status-badge');

      const gpsText = document.getElementById('gps-display-status');
      const gpsLat = document.getElementById('gps-lat');
      const gpsLon = document.getElementById('gps-lon');
      const gpsBadge = document.getElementById('gps-status-badge');
      const infoGpsStatus = document.getElementById('info-gps-status');

      const infoFaultType = document.getElementById('info-fault-type');
      const historyLog = document.getElementById('history-log');
      const mapsBtn = document.getElementById('maps-btn');

      // 1. Update Current
      currentValSpan.textContent = parseFloat(data.current).toFixed(2);
      
      // Visual scaling (Progress bar uses 30A max reference)
      const barPercentage = Math.min((data.current / 30.0) * 100, 100);
      currentBar.style.width = `${barPercentage}%`;

      const lowThresh = 16.40;
      const highThresh = 18.01;
      
      const isLineSnap = data.current < lowThresh;
      const isOvercurrent = data.current > highThresh;

      if (isOvercurrent) {
        cardCurrent.className = "card card-warning-active";
        currentBar.style.backgroundColor = "#EF4444";
        currentBadge.className = "badge badge-fault";
        currentBadge.textContent = "OVERCURRENT";
        rowCurrent.innerHTML = `<span class="badge badge-fault">OVERCURRENT</span>`;
      } else if (isLineSnap) {
        cardCurrent.className = "card card-warning-active";
        currentBar.style.backgroundColor = "#EF4444";
        currentBadge.className = "badge badge-fault";
        currentBadge.textContent = "LINE SNAP";
        rowCurrent.innerHTML = `<span class="badge badge-fault">LINE SNAP</span>`;
      } else {
        cardCurrent.className = "card";
        currentBar.style.backgroundColor = "#3B82F6";
        currentBadge.className = "badge badge-info";
        currentBadge.textContent = "RUNNING";
        rowCurrent.innerHTML = `<span class="badge badge-normal">NORMAL</span>`;
      }

      // 2. Update Tilt
      if (data.tilt === "FAULT") {
        cardTilt.className = "card card-warning-active";
        tiltText.textContent = "FAULT";
        tiltBadge.className = "badge badge-fault";
        tiltBadge.textContent = "FAULT";
        poleLine.setAttribute("transform", "rotate(15 12 21)");
        rowTilt.innerHTML = `<span class="badge badge-fault">FAULT</span>`;
      } else {
        cardTilt.className = "card";
        tiltText.textContent = "NORMAL";
        tiltBadge.className = "badge badge-normal";
        tiltBadge.textContent = "NORMAL";
        poleLine.removeAttribute("transform");
        rowTilt.innerHTML = `<span class="badge badge-normal">NORMAL</span>`;
      }

      // 3. Update Vibration
      if (data.vibration === "FAULT") {
        cardVibration.className = "card card-warning-active";
        vibrationText.textContent = "FAULT";
        vibrationBadge.className = "badge badge-fault";
        vibrationBadge.textContent = "FAULT";
        rowVibration.innerHTML = `<span class="badge badge-fault">FAULT</span>`;
      } else {
        cardVibration.className = "card";
        vibrationText.textContent = "NORMAL";
        vibrationBadge.className = "badge badge-normal";
        vibrationBadge.textContent = "NORMAL";
        rowVibration.innerHTML = `<span class="badge badge-normal">NORMAL</span>`;
      }

      // 4. Update GPS
      if (data.gpsValid === 1) {
        gpsText.textContent = "CONNECTED";
        gpsBadge.className = "badge badge-normal";
        gpsBadge.textContent = "CONNECTED";
        gpsLat.textContent = parseFloat(data.latitude).toFixed(6);
        gpsLon.textContent = parseFloat(data.longitude).toFixed(6);
        infoGpsStatus.textContent = "CONNECTED";
        infoGpsStatus.style.color = "#34D399";
        mapsBtn.style.display = "inline-flex";
        mapsBtn.href = `https://maps.google.com/?q=${data.latitude},${data.longitude}`;
      } else {
        gpsText.textContent = "NO FIX";
        gpsBadge.className = "badge badge-warning";
        gpsBadge.textContent = "NO FIX";
        gpsLat.textContent = "0.000000";
        gpsLon.textContent = "0.000000";
        infoGpsStatus.textContent = "NO FIX";
        infoGpsStatus.style.color = "#FCD34D";
        mapsBtn.style.display = "none";
      }

      // 5. Update Status Banner & Device Info
      infoFaultType.textContent = data.faultType;
      if (data.faultType === 0) {
        banner.className = "status-banner status-banner-normal";
        statusTitle.textContent = "SYSTEM NORMAL";
        statusDesc.textContent = "No abnormal conditions detected. Continuous monitoring active.";
        rowOverall.innerHTML = `<span class="badge badge-normal">SYSTEM NORMAL</span>`;
        infoFaultType.style.color = "#34D399";
      } else {
        banner.className = "status-banner status-banner-fault";
        statusTitle.textContent = "FAULT ENCOUNTERED";
        statusDesc.textContent = "Active Fault: " + data.status;
        rowOverall.innerHTML = `<span class="badge badge-fault">${data.status}</span>`;
        infoFaultType.style.color = "#FCA5A5";
      }

      // 6. Update History
      if (data.history.trim() !== "") {
        historyLog.innerHTML = data.history;
      } else {
        historyLog.innerHTML = "No logged alerts yet.<br>Monitoring active...";
      }
    }

    function fetchTelemetry() {
      fetch("/data")
        .then(response => response.json())
        .then(data => {
          updateDashboard(data);
        })
        .catch(err => {
          console.error("Dashboard telemetry sync error:", err);
          document.getElementById('clock-display').textContent = "DISCONNECTED";
        });
    }

    // Poll server every 1000ms
    setInterval(fetchTelemetry, 1000);
    // Initial call on load
    window.addEventListener('DOMContentLoaded', fetchTelemetry);
  </script>
</body>
</html>
)rawliteral";

//----------------------------------------------------
// Read Current Sensor
//----------------------------------------------------
float readCurrent()
{
  long sum = 0;

  for(int i = 0; i < 500; i++)
  {
    sum += analogRead(CURRENT_PIN);
    delayMicroseconds(200);
  }

  float adc = sum / 500.0;
  float voltage = adc * 3.3 / 4095.0;
  float amps = abs((voltage - zeroVoltage) / sensitivity);

  if(amps < 0.02) // Noise floor threshold
    amps = 0;

  return amps;
}

//----------------------------------------------------
// Calibrate Current Sensor Zero-Point
//----------------------------------------------------
void calibrateCurrentSensor()
{
  Serial.println("Calibrating current sensor... keep load OFF");
  delay(1000);

  long sum = 0;
  const int samples = 2000;

  for(int i = 0; i < samples; i++)
  {
    sum += analogRead(CURRENT_PIN);
    delayMicroseconds(200);
  }

  float adc = sum / (float)samples;
  zeroVoltage = adc * 3.3 / 4095.0;

  Serial.print("Calibrated zeroVoltage = ");
  Serial.println(zeroVoltage, 4);
}

//----------------------------------------------------
// Read GPS Telemetry
//----------------------------------------------------
void readGPS()
{
  while(GPSserial.available())
  {
    gps.encode(GPSserial.read());
  }

  if(gps.location.isValid())
  {
    gpsValid = true;
    latitude = gps.location.lat();
    longitude = gps.location.lng();
  }
  else
  {
    gpsValid = false;
  }
}

//----------------------------------------------------
// Fault Diagnostics Logic
//----------------------------------------------------
void checkFaults()
{
  current = readCurrent();

  bool lineSnap   = current < LOW_CURRENT_THRESHOLD;
  bool overCurrent = current > HIGH_CURRENT_THRESHOLD;

  tiltFault      = (digitalRead(TILT_PIN) == HIGH);      // HIGH = fault
  vibrationFault = (digitalRead(VIBRATION_PIN) == HIGH); // HIGH = fault

  currentFault = lineSnap || overCurrent;

  int activeCount = (lineSnap ? 1 : 0) + (overCurrent ? 1 : 0) + (tiltFault ? 1 : 0) + (vibrationFault ? 1 : 0);

  if (activeCount == 0)
  {
    faultStatus = "NO FAULT";
    faultType = 0;
  }
  else if (activeCount == 1)
  {
    if (lineSnap)          { faultStatus = "LINE SNAP";       faultType = 1; }
    else if (tiltFault)    { faultStatus = "TILT FAULT";      faultType = 2; }
    else if (vibrationFault) { faultStatus = "VIBRATION FAULT"; faultType = 3; }
    else if (overCurrent)    { faultStatus = "OVERCURRENT";     faultType = 4; }
  }
  else
  {
    // Multiple concurrent faults detected
    String combo = "";
    if (lineSnap)          combo += "LINE SNAP + ";
    if (overCurrent)       combo += "OVERCURRENT + ";
    if (tiltFault)         combo += "TILT + ";
    if (vibrationFault)    combo += "VIBRATION + ";
    combo.remove(combo.length() - 3); // Trim trailing " + "

    faultStatus = "MULTIPLE FAULTS: " + combo;
    faultType = 5;
  }
}

//----------------------------------------------------
// Fault Log & History Management
//----------------------------------------------------
void updateFaultHistory()
{
  static String lastFault = "";
  static unsigned long lastLogTime = 0;

  if (faultStatus != "NO FAULT" &&
      faultStatus != lastFault &&
      millis() - lastLogTime > 2000)
  {
    String log = "";

    log += "[";
    log += String(millis() / 1000);
    log += "s] ";
    log += faultStatus;
    log += "<br>"; // HTML newline for webpage formatting

    faultHistory = log + faultHistory;

    lastFault = faultStatus;
    lastLogTime = millis();
  }

  if (faultStatus == "NO FAULT")
  {
    lastFault = "";
  }
}

//----------------------------------------------------
// ESP-NOW Tx Callback
//----------------------------------------------------
void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
    {
        Serial.println("ESP-NOW: Transmission Success");
    }
    else
    {
        Serial.println("ESP-NOW: Transmission Failed");
    }
}

//----------------------------------------------------
// Broadcast Fault State via ESP-NOW
//----------------------------------------------------
void sendFault()
{
  outgoingData.fault = (faultType != 0);
  outgoingData.type = faultType;

  esp_now_send(receiverMAC, (uint8_t *)&outgoingData, sizeof(outgoingData));
}

//----------------------------------------------------
// AJAX Telemetry Payload served at /data
//----------------------------------------------------
void handleData()
{
  String json = "{";
  json += "\"current\":" + String(current, 2);
  json += ",\"load\":\"" + String(current > LOW_CURRENT_THRESHOLD ? "RUNNING" : "OFF") + "\"";
  json += ",\"tilt\":\"" + String(tiltFault ? "FAULT" : "NORMAL") + "\"";
  json += ",\"vibration\":\"" + String(vibrationFault ? "FAULT" : "NORMAL") + "\"";
  json += ",\"latitude\":" + String(latitude, 6);
  json += ",\"longitude\":" + String(longitude, 6);
  json += ",\"status\":\"" + faultStatus + "\"";
  json += ",\"history\":\"" + faultHistory + "\"";
  json += ",\"gpsValid\":" + String(gpsValid ? 1 : 0);
  json += ",\"faultType\":" + String(faultType);
  json += ",\"poleId\":\"" + POLE_ID + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

//----------------------------------------------------
// Serve Main Webpage
//----------------------------------------------------
void handleRoot()
{
  server.send(200, "text/html", webpage);
}

//----------------------------------------------------
// Setup Configuration
//----------------------------------------------------
void setup()
{
  Serial.begin(115200);

  pinMode(CURRENT_PIN, INPUT);
  pinMode(VIBRATION_PIN, INPUT);
  pinMode(TILT_PIN, INPUT);

  // Calibrate ACS712 sensor offsets (Make sure load is OFF at startup)
  calibrateCurrentSensor();

  // Initialize GPS UART
  GPSserial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  // Initialize Wi-Fi in coexisting STA/AP mode for ESP-NOW and Web Server
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.println("=========================================");
  Serial.println("   SMART TRANSMISSION LINE PROTECTION   ");
  Serial.println("        ESP32 PROTECTION UNIT ACTIVE     ");
  Serial.print("   SCADA Dashboard: http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("=========================================");

  // Initialize ESP-NOW Protocol
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW Init Failure!");
  }
  else
  {
    Serial.println("ESP-NOW Activated");
  }

  esp_now_register_send_cb(onDataSent);

  // Add Breaker ESP32 Peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) == ESP_OK)
  {
    Serial.println("Breaker ESP32 Peer Registered successfully");
  }
  else
  {
    Serial.println("Breaker ESP32 Peer Registration Failed");
  }

  // Define Server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);

  server.begin();
  Serial.println("SCADA Web Server Listening...");
}

//----------------------------------------------------
// Main Execution Loop
//----------------------------------------------------
void loop()
{
  // Continuously ingest GPS packets to avoid buffer overflows
  readGPS();

  // Handle client requests immediately
  server.handleClient();

  // Run protection routines at regular intervals (telemetryInterval = 500ms)
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= telemetryInterval)
  {
    previousMillis = currentMillis;
    
    // Evaluate sensor reads and diagnostics
    checkFaults();

    // Append to live register log
    updateFaultHistory();

    // Broadcast status to control breaker
    sendFault();
  }
}
