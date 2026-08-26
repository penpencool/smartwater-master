#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

#include "espnow_types.h"

// ==========================================
// 1. PIN DEFINITIONS (Node 3 Tank Sensor)
// ==========================================
#define PIN_TRIG            5   // JSN-SR04T Trig Pin
#define PIN_ECHO            18  // JSN-SR04T Echo Pin
#define PIN_FLOAT_BACKUP    19  // Magnetic Float Switch (Backup / Overflow) - Pull-up
#define PIN_BATTERY_ADC     34  // Battery Voltage Divider (ADC Input)
#define PIN_LED_STATUS      2   // Onboard Blue LED

// ==========================================
// 2. TANK CALIBRATION PARAMETERS & NVS
// ==========================================
// ค่าเริ่มต้น Calibration (เซนติเมตร)
float distTankEmptyCm = 180.0f; // แทงค์แห้ง (0%) ระยะไกลสุด
float distTankFullCm  = 25.0f;  // แทงค์เต็ม (100%) ระยะใกล้หัวเซ็นเซอร์

Preferences prefs;

// Broadcast MAC Address for ESP-NOW (ส่งแบบ Broadcast ให้ทุกตัวที่ฟังอยู่)
uint8_t broadcastMacAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

TankNodePayload payload;
uint32_t msgCount = 0;

// ==========================================
// 3. ROBUST SENSOR READING FUNCTIONS (JSN-SR04T V3.0)
// ==========================================
float readSinglePingDistanceCm() {
    // 1. เคลียร์ Trigger Pin ให้ชัวร์ก่อน
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(5);

    // 2. ส่ง Pulse Trigger ความกว้าง 20us (JSN-SR04T V3.0 แนะนำ 20us ขึ้นไปเพื่อความเสถียร)
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(20);
    digitalWrite(PIN_TRIG, LOW);

    // 3. ใช้ pulseInLong บน ESP32 พร้อม Timeout 35,000us (~6 เมตร)
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

float calculateWaterPercent(float distanceCm) {
    if (distanceCm < 0) return (payload.waterLevelPercent > 0) ? payload.waterLevelPercent : 0.0f;

    // ป้องกันกรณีผู้ใช้ตั้งค่า Empty <= Full
    if (distTankEmptyCm <= distTankFullCm) return 0.0f;

    // Clamp distance within calibration range
    if (distanceCm >= distTankEmptyCm) return 0.0f;
    if (distanceCm <= distTankFullCm) return 100.0f;

    // Linear Interpolation: 0% at Empty, 100% at Full
    float percent = ((distTankEmptyCm - distanceCm) / (distTankEmptyCm - distTankFullCm)) * 100.0f;
    return constrain(percent, 0.0f, 100.0f);
}

float readBatteryVoltage() {
    // อัตราทด Voltage Divider (เช่น R1=100k, R2=100k -> x2.0)
    int rawAdc = analogRead(PIN_BATTERY_ADC);
    float pinVolt = (rawAdc / 4095.0f) * 3.3f;
    float batVolt = pinVolt * 2.0f; 
    return batVolt;
}

// ==========================================
// 4. ESP-NOW RECEIVER (รับคำสั่ง Calibration จาก Master)
// ==========================================
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void onDataReceived(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
#else
void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
    if (len == sizeof(TankCalibrationPayload)) {
        TankCalibrationPayload calib;
        memcpy(&calib, incomingData, sizeof(TankCalibrationPayload));

        if (calib.msgType == NODE_TANK_CALIBRATE) {
            Serial.printf("📥 Received Calibration: Empty=%.1f cm, Full=%.1f cm\n", calib.distEmptyCm, calib.distFullCm);
            distTankEmptyCm = calib.distEmptyCm;
            distTankFullCm = calib.distFullCm;

            // บันทึกลง Flash NVS ถาวร
            prefs.putFloat("distEmpty", distTankEmptyCm);
            prefs.putFloat("distFull", distTankFullCm);

            // กะพริบไฟ LED 3 ครั้งยืนยันการบันทึก
            for (int i = 0; i < 3; i++) {
                digitalWrite(PIN_LED_STATUS, HIGH);
                delay(100);
                digitalWrite(PIN_LED_STATUS, LOW);
                delay(100);
            }
        }
    }
}

// ==========================================
// 5. SETUP & LOOP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("==========================================");
    Serial.println("Starting Node 3: Water Tank Sensor (JSN-SR04T V3.0)...");
    Serial.printf("Configured Pins: Trig=GPIO %d, Echo=GPIO %d\n", PIN_TRIG, PIN_ECHO);
    Serial.println("==========================================");

    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    pinMode(PIN_FLOAT_BACKUP, INPUT_PULLUP);
    pinMode(PIN_LED_STATUS, OUTPUT);

    // Initial Pins
    digitalWrite(PIN_TRIG, LOW);
    digitalWrite(PIN_LED_STATUS, LOW);

    // Load Calibration from NVS
    prefs.begin("node3_calib", false);
    distTankEmptyCm = prefs.getFloat("distEmpty", 180.0f);
    distTankFullCm  = prefs.getFloat("distFull", 25.0f);
    Serial.printf("Loaded NVS Config: Empty=%.1f cm, Full=%.1f cm\n", distTankEmptyCm, distTankFullCm);

    // Wi-Fi Station Mode for ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed!");
        return;
    }

    // Register Receive Callback สำหรับรับคำสั่ง Calibration
    esp_now_register_recv_cb(onDataReceived);

    // Register Broadcast Peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMacAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add broadcast peer");
    }

    Serial.println("Node 3 Ready (Broadcast Mode).");
}

void loop() {
    // 1. อ่านค่าระยะ Ultrasonic
    float distanceCm = readUltrasonicDistanceCm();
    float waterPct = calculateWaterPercent(distanceCm);

    // 2. อ่านสวิตช์ลูกลอย Backup (Active LOW เมื่อลูกลอยต่อลง GND ตอนลอยสูงสุด)
    bool floatActive = (digitalRead(PIN_FLOAT_BACKUP) == LOW);

    // 3. อ่านแรงดันแบตเตอรี่
    float batV = readBatteryVoltage();

    // 4. บรรจุ Payload
    payload.nodeId = NODE_WATER_TANK;
    payload.waterLevelPercent = waterPct;
    payload.distanceCm = (distanceCm > 0) ? distanceCm : 0.0f;
    payload.floatBackupActive = floatActive;
    payload.batteryVoltage = batV;
    payload.currentEmptyCm = distTankEmptyCm;
    payload.currentFullCm = distTankFullCm;
    payload.messageId = ++msgCount;

    // 5. ส่ง ESP-NOW Broadcast เข้า Master Controller
    digitalWrite(PIN_LED_STATUS, HIGH); // ไฟสถานะกะพริบขณะส่ง
    esp_err_t result = esp_now_send(broadcastMacAddress, (uint8_t *)&payload, sizeof(payload));
    digitalWrite(PIN_LED_STATUS, LOW);

    if (distanceCm > 0) {
        Serial.printf("[Node 3] Level: %.1f %% | Dist: %.1f cm (Empty:%.0f/Full:%.0f) | Float: %s | Bat: %.2fV | Sent: %s\n",
                      waterPct, distanceCm, distTankEmptyCm, distTankFullCm, floatActive ? "ACTIVE" : "NORMAL", batV,
                      (result == ESP_OK) ? "OK" : "FAIL");
    } else {
        Serial.printf("[Node 3] ⚠️ No Echo / Sensor Read Error (Check 5V Power & Wiring Trig:G5, Echo:G18) | Bat: %.2fV | Sent: %s\n",
                      batV, (result == ESP_OK) ? "OK" : "FAIL");
    }

    // ส่งข้อมูลทุก 5 วินาทีระหว่างทดสอบ (ปกติ 15 วิ)
    delay(5000);
}
