#include <WiFi.h>
#include <esp_now.h>

#define RELAY_PIN 26

typedef struct
{
  bool fault;
  int type;
} message;

message incomingData;

// Breaker status
bool breakerTripped = false;

// ESP-NOW Receive Callback
void onReceive(const esp_now_recv_info_t *recv_info,
               const uint8_t *data,
               int len)
{
  memcpy(&incomingData, data, sizeof(incomingData));

  Serial.println();
  Serial.println("================================");
  Serial.print("Fault Status : ");
  Serial.println(incomingData.fault);

  Serial.print("Fault Type   : ");
  Serial.println(incomingData.type);

  // Trip only once
  if (incomingData.fault && !breakerTripped)
  {
    breakerTripped = true;

    // Active LOW relay
    // HIGH = Relay OFF = Breaker TRIPPED
    digitalWrite(RELAY_PIN, HIGH);

    Serial.println("--------------------------------");
    Serial.println("BREAKER TRIPPED");

    switch (incomingData.type)
    {
      case 1:
        Serial.println("FAULT : LINE SNAP");
        break;

      case 2:
        Serial.println("FAULT : TILT DETECTED");
        break;

      case 3:
        Serial.println("FAULT : VIBRATION DETECTED");
        break;

      case 4:
        Serial.println("FAULT : OVERCURRENT");
        break;

      case 5:
        Serial.println("FAULT : MULTIPLE FAULTS");
        break;

      default:
        Serial.println("FAULT : UNKNOWN");
        break;
    }

    Serial.println();
    Serial.println(">>> Waiting for Manual Reset <<<");
    Serial.println("Restart ESP32 after inspection.");
  }
  else if (!incomingData.fault)
  {
    Serial.println("System Normal");
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);

  // Active LOW relay
  // LOW = Relay ON = Breaker Closed
  digitalWrite(RELAY_PIN, LOW);

  Serial.println();
  Serial.println("==================================");
  Serial.println(" Smart Breaker Unit Started");
  Serial.println(" Breaker Status : CLOSED");
  Serial.println(" Waiting for Fault...");
  Serial.println("==================================");

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("ESP-NOW Initialization Failed!");
    return;
  }

  esp_now_register_recv_cb(onReceive);
}

void loop()
{
  // Nothing required
}