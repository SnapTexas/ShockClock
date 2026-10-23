/*
  Clock Control MQTT integration template for ESP32 + HiveMQ Cloud.

  Libraries:
    - PubSubClient by Nick O'Leary
    - ArduinoJson (v6 or v7)
  Board: ESP32

  Merge these parts into your existing clock sketch; don't upload this as a
  replacement if your existing sketch already owns setup(), loop(), Wi-Fi,
  RTC, OLED, and MAX7219 initialization.

  Web UI publishes JSON to "clock/control".
*/
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <string.h>

// ---- Fill in these values; use a dedicated, restricted MQTT account ----
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* MQTT_HOST = "YOUR_CLUSTER.s1.eu.hivemq.cloud";
const uint16_t MQTT_PORT = 8883;
const char* MQTT_USERNAME = "YOUR_MQTT_USERNAME";
const char* MQTT_PASSWORD = "YOUR_MQTT_PASSWORD";
const char* MQTT_TOPIC = "clock/control";

// TLS note: setInsecure() is convenient for a quick prototype but does not
// verify the broker certificate. Replace with setCACert(root_ca) for deployment.
WiFiClientSecure mqttSecureClient;
PubSubClient mqttClient(mqttSecureClient);

String webMessage;
bool hasWebMessage = false;
bool hasWebMatrix = false;
uint8_t webMatrix[8][8] = {{0}};
volatile bool remoteTriggerPending = false;

// ---- Connect these three functions to your existing clock implementation ----
void startExistingAlarmFromWeb() {
  // TODO: call the same function your clock uses when the alarm starts.
  // If your current firmware uses `alarm_ringing`, setting it true may be
  // sufficient ONLY if your existing loop uses that flag to start the sound.
  // Example (uncomment only if this matches your sketch):
  // alarm_ringing = true;
}

void showWebMessageOnOLED(const String& message) {
  // TODO: render `message` with your existing U8g2 display code.
  // For example, drawStr(...) between u8g2.clearBuffer() and sendBuffer().
}

void showWebMatrixOnMAX7219(const uint8_t matrix[8][8]) {
  // TODO: send the pattern to your existing MAX7219 library instance.
  // The row/column orientation depends on your module and library.
  // `matrix[row][col]` is 1 for ON and 0 for OFF.
}

// MQTT callback must stay out of ISR context; this is a normal network callback.
void mqttMessageCallback(char* topic, byte* payload, unsigned int length) {
  if (String(topic) != MQTT_TOPIC) return;

  // PubSubClient payload is not guaranteed to be null-terminated.
  StaticJsonDocument<1024> doc; // Works with ArduinoJson v6 and v7.
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("Bad MQTT JSON: ");
    Serial.println(error.c_str());
    return;
  }

  if (doc["type"] != "clock_control") return;

  if (doc["trigger"].is<bool>() && doc["trigger"].as<bool>()) {
    remoteTriggerPending = true;
  }

  if (doc["message"].is<const char*>()) {
    webMessage = doc["message"].as<String>();
    hasWebMessage = true;
  }

  JsonArray matrix = doc["matrix"].as<JsonArray>();
  if (!matrix.isNull() && matrix.size() == 8) {
    bool valid = true;
    uint8_t nextMatrix[8][8] = {{0}};
    for (uint8_t r = 0; r < 8; r++) {
      const char* row = matrix[r];
      if (row == nullptr || strlen(row) != 8) { valid = false; break; }
      for (uint8_t c = 0; c < 8; c++) {
        if (row[c] != '0' && row[c] != '1') { valid = false; break; }
        nextMatrix[r][c] = (row[c] == '1') ? 1 : 0;
      }
      if (!valid) break;
    }
    if (valid) {
      memcpy(webMatrix, nextMatrix, sizeof(webMatrix));
      hasWebMatrix = true;
    } else {
      Serial.println("Ignored invalid matrix (expected 8 rows of 8 binary digits).");
    }
  }

  Serial.print("Received clock command on ");
  Serial.println(topic);
}

void connectWiFiForClock() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print('.');
  }
  Serial.println();
  Serial.print("Wi-Fi IP: ");
  Serial.println(WiFi.localIP());
}

void reconnectClockMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  while (!mqttClient.connected()) {
    String clientId = "esp32-clock-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.print("Connecting MQTT...");
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
      if (!mqttClient.subscribe(MQTT_TOPIC, 1)) {
        Serial.println("Subscribe failed");
      }
    } else {
      Serial.print("failed, state=");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

/*
  Add to your existing setup(), after Serial.begin(...):
    connectWiFiForClock();
    mqttSecureClient.setInsecure(); // prototype only; use setCACert in production
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttMessageCallback);
    mqttClient.setBufferSize(1024);
    reconnectClockMQTT();

  Add to your existing loop():
    if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
      reconnectClockMQTT();
    }
    mqttClient.loop();

    // Handle commands in loop context, not inside an ISR:
    if (remoteTriggerPending) {
      remoteTriggerPending = false;
      startExistingAlarmFromWeb();
    }
    if (hasWebMessage) {
      hasWebMessage = false;
      showWebMessageOnOLED(webMessage);
    }
    if (hasWebMatrix) {
      hasWebMatrix = false;
      showWebMatrixOnMAX7219(webMatrix);
    }

  Notes:
    - If you need message/matrix to persist, store them using Preferences.
    - Trigger command is an immediate trigger, not a scheduled alarm time.
    - This template does not publish status back to the website.
*/
