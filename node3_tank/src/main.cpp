#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "espnow_types.h"

// ==========================================
// 1. PIN DEFINITIONS (Node 3 Tank Sensor)
// ==========================================
#define PIN_TRIG            5   // JSN-SR04T Trig Pin
#define PIN_ECHO            18  // JSN-SR04T Echo Pin
#define PIN_FLOAT_BACKUP    19  // Magnetic Float Switch (Backup / Overflow) - Pull-up
#define PIN_BATTERY_ADC     34  // Battery Voltage Divider (ADC Input)
#define PIN_LED_STATUS      2   // Onboard Blue LED

// Broadcast MAC Address for ESP-NOW (ส่งแบบ Broadcast ให้ Master Controller รับตรง)
uint8_t broadcastMacAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

TankNodePayload payload;
uint32_t msgCount = 0;

// ==========================================
// 2. ROBUST SENSOR READING FUNCTIONS (JSN-SR04T V3.0)
// ==========================================
float readSinglePingDistanceCm() {
    // 1. เคลียร์ Trigger Pin
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(5);

    // 2. ส่ง Pulse Trigger ความกว้าง 20us (JSN-SR04T V3.0 แนะนำ 20us ขึ้นไป)
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(20);
    digitalWrite(PIN_TRIG, LOW);

    // 3. ใช้ pulseInLong พร้อม Timeout 35,000us (~6 เมตร)
    unsigned long durationUs = pulseInLong(PIN_ECHO, HIGH, 35000);
    if (durationUs == 0) {
        return -1.0f; // Timeout / No Echo
    }

    // 4. คำนวณระยะทาง Speed of sound = 343 m/s = 0.0343 cm/us
    float distance = (durationUs * 0.0343f) / 2.0f;
    return distance;
}

// ฟังก์ชันอ่านระยะแบบกรองค่า (Median Filter) อ่าน 5 ครั้ง คัดเฉพาะค่าจริง
float readUltrasonicDistanceCm() {
    float samples[5];
    int validCount = 0;

    for (int i = 0; i < 5; i++) {
        float d = readSinglePingDistanceCm();
        // JSN-SR04T Blind zone อยู่ที่ประมาณ 20cm วัดได้ไกลสุด 450cm
        if (d >= 15.0f && d <= 450.0f) {
            samples[validCount++] = d;
        }
        delay(35); // หน่วงเวลา 35ms ให้คลื่นสะท้อนสลายตัว
    }

    if (validCount == 0) {
        return -1.0f; // อ่านไม่สำเร็จเลย
    }

    // เรียงลำดับตัวเลขเพื่อหาค่ามัธยฐาน (Median) ตัด Noise กระโดด
    for (int i = 0; i < validCount - 1; i++) {
        for (int j = i + 1; j < validCount; j++) {
            if (samples[i] > samples[j]) {
                float tmp = samples[i];
                samples[i] = samples[j];
                samples[j] = tmp;
            }
        }
    }

    return samples[validCount / 2];
}

float readBatteryVoltage() {
    // อัตราทด Voltage Divider (R1=100k, R2=100k -> x2.0)
    int rawAdc = analogRead(PIN_BATTERY_ADC);
    float pinVolt = (rawAdc / 4095.0f) * 3.3f;
    float batVolt = pinVolt * 2.0f; 
    return batVolt;
}

// ==========================================
// 3. SETUP & LOOP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("==========================================");
    Serial.println("Starting Node 3: Water Tank Sensor (Raw Distance CM)...");
    Serial.printf("Configured Pins: Trig=GPIO %d, Echo=GPIO %d\n", PIN_TRIG, PIN_ECHO);
    Serial.println("Note: Calibration & % Calculation are handled centrally by Master.");
    Serial.println("==========================================");

    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    pinMode(PIN_FLOAT_BACKUP, INPUT_PULLUP);
    pinMode(PIN_LED_STATUS, OUTPUT);

    // Initial Pins
    digitalWrite(PIN_TRIG, LOW);
    digitalWrite(PIN_LED_STATUS, LOW);

    // Wi-Fi Station Mode for ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Disable WiFi sleep for rock-solid ESP-NOW transmission
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed!");
        return;
    }

    // Register Broadcast Peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMacAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add broadcast peer");
    }

    Serial.println("Node 3 Ready (Transmitting raw cm to Master).");
}

void loop() {
    // 1. อ่านค่าระยะจริงจาก Ultrasonic (cm)
    float distanceCm = readUltrasonicDistanceCm();

    // 2. อ่านสวิตช์ลูกลอย Backup (Active LOW เมื่อลูกลอยต่อลง GND ตอนลอยสูงสุด)
    bool floatActive = (digitalRead(PIN_FLOAT_BACKUP) == LOW);

    // 3. อ่านแรงดันแบตเตอรี่
    float batV = readBatteryVoltage();

    // 4. บรรจุ Payload (ส่งค่าระยะ cm ไปให้ Master ประมวลผลและคำนวณระดับน้ำที่ส่วนกลาง)
    payload.nodeId = NODE_WATER_TANK;
    payload.waterLevelPercent = 0.0f; // ให้ Master คำนวณตามค่า Calibration ที่ Master
    payload.distanceCm = distanceCm;
    payload.floatBackupActive = floatActive;
    payload.batteryVoltage = batV;
    payload.currentEmptyCm = 0.0f;
    payload.currentFullCm = 0.0f;
    payload.messageId = ++msgCount;

    // 5. ส่ง ESP-NOW Broadcast เข้า Master Controller
    digitalWrite(PIN_LED_STATUS, HIGH); // ไฟสถานะกะพริบขณะส่ง
    esp_err_t result = esp_now_send(broadcastMacAddress, (uint8_t *)&payload, sizeof(payload));
    digitalWrite(PIN_LED_STATUS, LOW);

    if (distanceCm > 0) {
        Serial.printf("[Node 3] Raw Dist: %.1f cm | Float Backup: %s | Bat: %.2fV | Msg #%u (Sent: %s)\n",
                      distanceCm, floatActive ? "ACTIVE (Cutoff)" : "NORMAL", batV, msgCount,
                      (result == ESP_OK) ? "OK" : "FAIL");
    } else {
        Serial.printf("[Node 3] ⚠️ No Echo / Sensor Read Error (Check 5V Power & Wiring Trig:G5, Echo:G18) | Bat: %.2fV | Msg #%u (Sent: %s)\n",
                      batV, msgCount, (result == ESP_OK) ? "OK" : "FAIL");
    }

    // ส่งข้อมูลทุก 5 วินาทีระหว่างทดสอบ (ปกติ 15 วิ)
    delay(5000);
}
