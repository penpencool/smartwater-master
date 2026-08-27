#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_sleep.h>

#include "espnow_types.h"

// ==========================================
// 1. PIN DEFINITIONS (Node 3 Tank Sensor)
// ==========================================
#define PIN_TRIG            5   // JSN-SR04T Trig Pin
#define PIN_ECHO            18  // JSN-SR04T Echo Pin
#define PIN_FLOAT_BACKUP    19  // Magnetic Float Switch (Backup / Overflow) - Pull-up
#define PIN_BATTERY_ADC     34  // Battery Voltage Divider (ADC Input)
#define PIN_LED_STATUS      2   // Onboard Blue LED

// Broadcast MAC Address for ESP-NOW
uint8_t broadcastMacAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ==========================================
// 2. PERSISTENT RTC MEMORY (Across Deep Sleep)
// ==========================================
RTC_DATA_ATTR uint32_t rtcBootCount = 0;
RTC_DATA_ATTR uint32_t rtcMsgCount = 0;

// การตั้งค่าที่ Sync จาก Master (จำไว้ใน RTC RAM ไม่ต้องเขียน Flash)
RTC_DATA_ATTR bool rtcConfigValid = false;
RTC_DATA_ATTR bool rtcSleepScheduleEnabled = true;
RTC_DATA_ATTR uint8_t rtcActiveStartHour = 6;
RTC_DATA_ATTR uint8_t rtcActiveStartMin = 0;
RTC_DATA_ATTR uint8_t rtcActiveEndHour = 18;
RTC_DATA_ATTR uint8_t rtcActiveEndMin = 0;

RTC_DATA_ATTR uint16_t rtcNormalIntervalSec = 30; // ส่งปกติทุก 30 วินาที
RTC_DATA_ATTR uint16_t rtcFastIntervalSec = 3;    // ส่งถี่ทุก 3 วินาที (ตอนใกล้เต็ม / ปั๊มสูบน้ำเข้า)
RTC_DATA_ATTR float rtcFastThresholdPct = 80.0f;  // เกณฑ์เริ่มส่งถี่ 80%

RTC_DATA_ATTR bool rtcBoreholeRunning = false;    // ปั๊มบาดาลเปิดอยู่หรือไม่
RTC_DATA_ATTR uint8_t rtcCurrentHour = 12;
RTC_DATA_ATTR uint8_t rtcCurrentMin = 0;
RTC_DATA_ATTR bool rtcTimeSynced = false;

// Flags สำหรับการรันรอบปัจจุบัน
volatile bool syncReceived = false;

// ==========================================
// 3. SENSOR READING FUNCTIONS (JSN-SR04T V3.0)
// ==========================================
float readSinglePingDistanceCm() {
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(5);

    // Pulse Trigger ความกว้าง 20us
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(20);
    digitalWrite(PIN_TRIG, LOW);

    // Timeout 35,000us (~6 เมตร)
    unsigned long durationUs = pulseInLong(PIN_ECHO, HIGH, 35000);
    if (durationUs == 0) {
        return -1.0f;
    }

    // Speed of sound = 343 m/s = 0.0343 cm/us
    float distance = (durationUs * 0.0343f) / 2.0f;
    return distance;
}

float readUltrasonicDistanceCm() {
    float samples[5];
    int validCount = 0;

    for (int i = 0; i < 5; i++) {
        float d = readSinglePingDistanceCm();
        if (d >= 15.0f && d <= 450.0f) {
            samples[validCount++] = d;
        }
        delay(25); // หน่วงเวลาสั้นๆ ระหว่างตัวอย่าง
    }

    if (validCount == 0) {
        return -1.0f;
    }

    // Median Filter
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
    // Voltage Divider (R1=100k, R2=100k -> x2.0)
    int rawAdc = analogRead(PIN_BATTERY_ADC);
    float pinVolt = (rawAdc / 4095.0f) * 3.3f;
    float batVolt = pinVolt * 2.0f; 
    return batVolt;
}

// ==========================================
// 4. ESP-NOW RECEIVER CALLBACK (Sync Config From Master)
// ==========================================
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
void onDataReceived(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
#else
void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {
#endif
    if (len >= (int)sizeof(TankSyncConfigPayload)) {
        uint8_t msgType = incomingData[0];
        if (msgType == NODE_TANK_SYNC_CFG) {
            TankSyncConfigPayload syncData;
            memcpy(&syncData, incomingData, sizeof(TankSyncConfigPayload));

            // บันทึก Config ลง RTC RAM ทันที
            rtcConfigValid           = true;
            rtcSleepScheduleEnabled  = syncData.sleepScheduleEnabled;
            rtcActiveStartHour       = syncData.activeStartHour;
            rtcActiveStartMin        = syncData.activeStartMin;
            rtcActiveEndHour         = syncData.activeEndHour;
            rtcActiveEndMin          = syncData.activeEndMin;
            rtcNormalIntervalSec     = syncData.normalIntervalSec;
            rtcFastIntervalSec       = syncData.fastIntervalSec;
            rtcFastThresholdPct      = syncData.fastThresholdPct;
            rtcBoreholeRunning       = syncData.isBoreholeRunning;
            rtcCurrentHour           = syncData.currentHour;
            rtcCurrentMin            = syncData.currentMin;
            rtcTimeSynced            = syncData.isNtpSynced;

            syncReceived = true;
            Serial.printf("📥 [Node 3] Received Sync from Master: Time=%02d:%02d:%02d (NTP=%s) | Normal=%ds, Fast=%ds (Thresh=%.0f%%) | Borehole=%s\n",
                          syncData.currentHour, syncData.currentMin, syncData.currentSec,
                          syncData.isNtpSynced ? "YES" : "NO",
                          syncData.normalIntervalSec, syncData.fastIntervalSec, syncData.fastThresholdPct,
                          syncData.isBoreholeRunning ? "ON" : "OFF");
        }
    }
}

// ==========================================
// 5. MAIN LOGIC (Execute Once & Deep Sleep)
// ==========================================
void setup() {
    rtcBootCount++;
    Serial.begin(115200);
    delay(20);

    Serial.println("==========================================");
    Serial.printf("🚀 Node 3 Tank Sensor (Boot #%u) Woke Up!\n", rtcBootCount);

    pinMode(PIN_TRIG, OUTPUT);
    pinMode(PIN_ECHO, INPUT);
    pinMode(PIN_FLOAT_BACKUP, INPUT_PULLUP);
    pinMode(PIN_LED_STATUS, OUTPUT);

    digitalWrite(PIN_TRIG, LOW);
    digitalWrite(PIN_LED_STATUS, HIGH); // ติดไฟแสดงสถานะขณะตื่นทำงาน

    // 1. อ่านค่าเซ็นเซอร์ทั้งหมด
    float distanceCm = readUltrasonicDistanceCm();
    bool floatActive = (digitalRead(PIN_FLOAT_BACKUP) == LOW);
    float batV = readBatteryVoltage();

    // 2. เริ่มต้น ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(onDataReceived);

        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, broadcastMacAddress, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);

        // 3. เตรียม Payload
        TankNodePayload payload;
        payload.nodeId = NODE_WATER_TANK;
        payload.waterLevelPercent = 0.0f; // Master ส่วนกลางจะคำนวณจาก distanceCm
        payload.distanceCm = distanceCm;
        payload.floatBackupActive = floatActive;
        payload.batteryVoltage = batV;
        payload.currentEmptyCm = 0.0f;
        payload.currentFullCm = 0.0f;
        payload.messageId = ++rtcMsgCount;

        // 4. ส่งข้อมูลแบบ Multi-Channel Broadcast
        esp_err_t sendResult = ESP_FAIL;
        for (uint8_t ch = 1; ch <= 13; ch++) {
            esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
            esp_err_t res = esp_now_send(broadcastMacAddress, (uint8_t *)&payload, sizeof(payload));
            if (res == ESP_OK) sendResult = ESP_OK;
            delay(1);
        }

        Serial.printf("📡 [Node 3] Sent Dist: %.1fcm | Float: %s | Bat: %.2fV | Msg #%u (Sent: %s)\n",
                      distanceCm, floatActive ? "ACTIVE" : "NORMAL", batV, rtcMsgCount,
                      (sendResult == ESP_OK) ? "OK" : "FAIL");

        // 5. รอรับแพ็กเก็ต Sync ตอบกลับจาก Master ชั่วครู่ (~60ms)
        unsigned long waitStart = millis();
        while (!syncReceived && (millis() - waitStart < 60)) {
            delay(5);
        }
    }

    digitalWrite(PIN_LED_STATUS, LOW);

    // ==========================================
    // 6. คำนวณระยะเวลา DEEP SLEEP อย่างชาญฉลาด
    // ==========================================
    uint32_t sleepSeconds = rtcNormalIntervalSec;
    if (sleepSeconds == 0) sleepSeconds = 30;

    // ตรวจสอบเงื่อนไขการทำงาน:
    // A. เช็คช่วงเวลากลางคืน / นอกเวลาทำงาน (เช่น 18:00 - 06:00)
    if (rtcSleepScheduleEnabled && rtcTimeSynced) {
        int nowM = rtcCurrentHour * 60 + rtcCurrentMin;
        int startM = rtcActiveStartHour * 60 + rtcActiveStartMin;
        int endM = rtcActiveEndHour * 60 + rtcActiveEndMin;

        bool isOffHours = false;
        int minsUntilWake = 0;

        if (startM < endM) {
            // ช่วงเวลาทำงานกลางวัน เช่น 06:00 - 18:00
            if (nowM >= endM) {
                isOffHours = true;
                minsUntilWake = (24 * 60 - nowM) + startM;
            } else if (nowM < startM) {
                isOffHours = true;
                minsUntilWake = startM - nowM;
            }
        } else if (startM > endM) {
            // ช่วงเวลาทำงานข้ามคืน เช่น 18:00 - 06:00
            if (nowM >= endM && nowM < startM) {
                isOffHours = true;
                minsUntilWake = startM - nowM;
            }
        }

        if (isOffHours && minsUntilWake > 0) {
            // นอกเวลาทำงาน: หลับยาวจนถึงเวลาเริ่มทำงาน (เช่น 06:00)
            sleepSeconds = (uint32_t)minsUntilWake * 60;
            Serial.printf("🌙 [Node 3] Off-hours detected (%02d:%02d - %02d:%02d). Sleeping for %d mins until %02d:%02d...\n",
                          rtcActiveStartHour, rtcActiveStartMin, rtcActiveEndHour, rtcActiveEndMin,
                          minsUntilWake, rtcActiveStartHour, rtcActiveStartMin);
        }
    }

    // B. ในเวลาทำงานปกติ: เลือกระหว่าง Normal Sampling vs Fast Sampling
    if (sleepSeconds == rtcNormalIntervalSec) {
        bool isFastMode = false;

        // เงื่อนไข 1: ปั๊มบาดาลเปิดอยู่ (กำลังเติมน้ำเข้าแทงค์)
        if (rtcBoreholeRunning) {
            isFastMode = true;
            Serial.println("⚡ [Node 3] Borehole pump active -> Switching to FAST SAMPLING.");
        }
        // เงื่อนไข 2: สวิตช์ลูกลอยตัดเตือนน้ำล้น
        else if (floatActive) {
            isFastMode = true;
            Serial.println("⚡ [Node 3] Float switch backup triggered -> Switching to FAST SAMPLING.");
        }
        // เงื่อนไข 3: ระยะน้ำอยู่ระดับใกล้เต็ม (เช่น < 50cm)
        else if (distanceCm > 0 && distanceCm <= 50.0f) {
            isFastMode = true;
            Serial.printf("⚡ [Node 3] Water near full (Dist: %.1fcm) -> Switching to FAST SAMPLING.\n", distanceCm);
        }

        if (isFastMode) {
            sleepSeconds = rtcFastIntervalSec;
            if (sleepSeconds == 0) sleepSeconds = 3;
        }
    }

    Serial.printf("💤 [Node 3] Entering Deep Sleep for %u seconds...\n", sleepSeconds);
    Serial.println("==========================================");
    Serial.flush();

    // 7. เข้าสู่ Deep Sleep ทันที (กินกระแส < 50 µA)
    esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds * 1000000ULL);
    esp_deep_sleep_start();
}

void loop() {
    // ไม่ได้ใช้งานเนื่องจาก ESP32 เข้าสู่ Deep Sleep ใน setup()
}
