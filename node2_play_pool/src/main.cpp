#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "../common/espnow_types.h"

// ==========================================
// 1. PIN DEFINITIONS (Node 2 - Play Pool)
// ==========================================
#define PIN_FLOAT_SWITCH    19  // Stainless Magnetic Float Switch (Pull-up)
#define PIN_BATTERY_ADC     34  // Battery Voltage Divider (ADC Input)
#define PIN_LED_STATUS      2   // Onboard Blue LED

// Broadcast MAC Address for ESP-NOW
uint8_t broadcastMacAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

PoolNodePayload payload;
uint32_t msgCount = 0;

// ==========================================
// 2. HELPER FUNCTIONS
// ==========================================
float readBatteryVoltage() {
    // Voltage Divider (100k / 100k -> x2.0 factor)
    int raw = analogRead(PIN_BATTERY_ADC);
    float vOut = (raw / 4095.0f) * 3.3f;
    float vBat = vOut * 2.0f; // Scale to 18650 (3.0V - 4.2V)
    return vBat;
}

// อ่านสถานะลูกลอยพร้อม Debounce ป้องกันคลื่นน้ำกระเพื่อม
bool readFloatSwitchDebounced() {
    int activeCount = 0;
    for (int i = 0; i < 10; i++) {
        // ลูกลอยตก (ระดับน้ำลดลง) = สวิตช์ดึงลง GND (LOW)
        if (digitalRead(PIN_FLOAT_SWITCH) == LOW) {
            activeCount++;
        }
        delay(20);
    }
    // หากเกิน 7 ใน 10 ครั้ง ถือว่าระดับน้ำลดลงจริง (ต้องการเติมน้ำ)
    return (activeCount >= 7);
}

void blinkStatusLed(int count = 1, int durationMs = 80) {
    for (int i = 0; i < count; i++) {
        digitalWrite(PIN_LED_STATUS, HIGH);
        delay(durationMs);
        digitalWrite(PIN_LED_STATUS, LOW);
        if (i < count - 1) delay(durationMs);
    }
}

// ==========================================
// 3. SETUP & LOOP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);

    pinMode(PIN_FLOAT_SWITCH, INPUT_PULLUP);
    pinMode(PIN_BATTERY_ADC, INPUT);
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    // WiFi Station Mode (ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Disable WiFi Sleep for solid transmission
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Initialize ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW Init Failed!");
        return;
    }

    // Register Broadcast Peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMacAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Failed to add broadcast peer!");
    } else {
        Serial.println("✅ ESP-NOW Broadcast Peer registered.");
    }

    Serial.println("==========================================");
    Serial.println("🏊 Node 2 (Play Pool Float Sensor) READY!");
    Serial.println("==========================================");
}

void loop() {
    bool waterLow = readFloatSwitchDebounced();
    float batV = readBatteryVoltage();
    msgCount++;

    payload.nodeId = NODE_PLAY_POOL; // 2
    payload.waterLow = waterLow;
    payload.batteryVoltage = batV;
    payload.messageId = msgCount;

    // Send Broadcast Payload via ESP-NOW
    esp_err_t result = esp_now_send(broadcastMacAddress, (uint8_t *)&payload, sizeof(payload));

    if (result == ESP_OK) {
        blinkStatusLed(1, 60);
        Serial.printf("📡 [Node 2] Sent: WaterLow=%s, Bat=%.2fV, MsgId=%u (SUCCESS)\n",
                      waterLow ? "YES (Need Fill)" : "NO (Full/OK)", batV, msgCount);
    } else {
        Serial.println("❌ [Node 2] ESP-NOW Send Failed!");
    }

    // ส่งข้อมูลทุกๆ 15 วินาที
    delay(15000);
}
