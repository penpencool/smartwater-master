#ifndef SYSTEM_NETWORK_H
#define SYSTEM_NETWORK_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <time.h>

#include "system_pins.h"
#include "system_state.h"
#include "config_manager.h"
#include "hardware_controller.h"

class SystemNetwork {
public:
    static unsigned long apStartTime;
    static bool apActive;
    static const unsigned long AP_TIMEOUT_MS = 60000; // 1 นาที
    static unsigned long lastNtpAttempt;

    static void syncNtpTime() {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("⏰ Configuring NTP Time with pool.ntp.org / time.google.com...");
            configTzTime("ICT-7", "pool.ntp.org", "time.google.com", "time.nist.gov");

            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 500)) {
                StateLock lock;
                ntpSynced = true;
                if (currentError == ERR_NTP_SYNC) currentError = ERR_NONE;
                Serial.printf("✅ NTP Time Synced: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            } else {
                StateLock lock;
                ntpSynced = false;
                Serial.println("ℹ️ NTP Sync pending (will retry in background)...");
            }
        } else {
            StateLock lock;
            ntpSynced = false;
        }
    }

    static void onWiFiEvent(WiFiEvent_t event) {
        switch (event) {
            case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                Serial.printf("🌐 [Wi-Fi STA] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
                syncNtpTime();
                break;
            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                Serial.println("⚠️ [Wi-Fi STA] Disconnected from AP. Reconnecting...");
                {
                    StateLock lock;
                    ntpSynced = false;
                }
                break;
            default:
                break;
        }
    }

    static String getCurrentTimeString() {
        if (!ntpSynced) return "--:--:--";
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 5)) {
            return "--:--:--";
        }
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
        return String(timeStr);
    }

    #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    static void onDataReceived(const esp_now_recv_info *recvInfo, const uint8_t *incomingData, int len) {
    #else
    static void onDataReceived(const uint8_t *mac, const uint8_t *incomingData, int len) {
    #endif
        if (len >= 1) {
            uint8_t nodeId = incomingData[0];

            // ✅ Fix: ได้ timestamp String ก่อน lock เพื่อลด mutex hold time
            String nowTime = getCurrentTimeString();

            // ✅ Fix: Lock เฉพาะตอน write global state เท่านั้น (ไม่รวม Serial.printf)
            if (nodeId == NODE_WAVE_POOL && len >= sizeof(PoolNodePayload)) {
                bool wLow; float bat;
                {
                    StateLock lock;
                    memcpy(&pool1Data, incomingData, sizeof(PoolNodePayload));
                    lastNode1Time = millis();
                    lastNode1TimeStr = nowTime;
                    wLow = pool1Data.waterLow;
                    bat  = pool1Data.batteryVoltage;
                }
                Serial.printf("📥 [Master] Recv Node 1 (Wave Pool @ %s): Low=%s, Bat=%.2fV\n",
                              nowTime.c_str(), wLow ? "YES" : "NO", bat);
            }
            else if (nodeId == NODE_PLAY_POOL && len >= sizeof(PoolNodePayload)) {
                bool wLow; float bat;
                {
                    StateLock lock;
                    memcpy(&pool2Data, incomingData, sizeof(PoolNodePayload));
                    lastNode2Time = millis();
                    lastNode2TimeStr = nowTime;
                    wLow = pool2Data.waterLow;
                    bat  = pool2Data.batteryVoltage;
                }
                Serial.printf("📥 [Master] Recv Node 2 (Play Pool @ %s): Low=%s, Bat=%.2fV\n",
                              nowTime.c_str(), wLow ? "YES" : "NO", bat);
            }
            else if (nodeId == NODE_WATER_TANK && len >= sizeof(TankNodePayload)) {
                // Copy raw bytes ก่อนโดยไม่ต้อง lock (incomingData คือ temp buffer ของ ESP-NOW)
                TankNodePayload localTank;
                memcpy(&localTank, incomingData, sizeof(TankNodePayload));

                // คำนวณ % ก่อน lock (ใช้ config อ่านอย่างเดียว ปลอดภัย)
                float emptyCm = configManager.config.tankEmptyCm;
                float fullCm  = configManager.config.tankFullCm;
                if (localTank.distanceCm > 0.0f && emptyCm > fullCm) {
                    if (localTank.distanceCm >= emptyCm) {
                        localTank.waterLevelPercent = 0.0f;
                    } else if (localTank.distanceCm <= fullCm) {
                        localTank.waterLevelPercent = 100.0f;
                    } else {
                        localTank.waterLevelPercent = ((emptyCm - localTank.distanceCm) / (emptyCm - fullCm)) * 100.0f;
                    }
                } else if (localTank.distanceCm <= 0.0f) {
                    // เซ็นเซอร์อ่านไม่สำเร็จ (No Echo / Read Error)
                    localTank.waterLevelPercent = 0.0f;
                }

                // ✅ Lock เฉพาะตอน write global tankData
                bool doAutoRecovery = false;
                bool doFloatCutoff  = false;
                {
                    StateLock lock;
                    tankData = localTank;
                    lastNode3Time    = millis();
                    lastNode3TimeStr = nowTime;
                    if (currentError == ERR_NODE_LOST) {
                        currentError = ERR_NONE;
                        doAutoRecovery = true;
                    }
                    if (localTank.floatBackupActive && stateBorehole) {
                        stateBorehole = false;
                        doFloatCutoff = true;
                    }
                }

                // ✅ Serial + HW calls นอก lock ทั้งหมด
                if (doAutoRecovery) {
                    Serial.println("🟢 [ESP-NOW Recv Node 3] Auto-cleared Alarm E3 (Sensor Reconnected)!");
                }
                Serial.printf("📥 [Master] Recv Node 3 @ %s: Raw Dist=%.1fcm -> Master Calc Level=%.1f%% (Empty:%.0f/Full:%.0f), Float=%s, Bat=%.2fV\n",
                              nowTime.c_str(), localTank.distanceCm, localTank.waterLevelPercent,
                              emptyCm, fullCm,
                              localTank.floatBackupActive ? "ACTIVE" : "NORMAL",
                              localTank.batteryVoltage);
                if (doFloatCutoff) {
                    HardwareController::setRelay(RELAY_BOREHOLE, false);
                }

                // 📡 Master ตอบกลับคำสั่ง Sync เวลา และ การตั้งค่า Sleep/Interval ให้ Node 3 ทันที
                TankSyncConfigPayload syncPayload;
                syncPayload.msgType = NODE_TANK_SYNC_CFG;
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {
                    syncPayload.currentHour = timeinfo.tm_hour;
                    syncPayload.currentMin  = timeinfo.tm_min;
                    syncPayload.currentSec  = timeinfo.tm_sec;
                    syncPayload.isNtpSynced = ntpSynced;
                } else {
                    syncPayload.currentHour = 12;
                    syncPayload.currentMin  = 0;
                    syncPayload.currentSec  = 0;
                    syncPayload.isNtpSynced = false;
                }
                syncPayload.sleepScheduleEnabled = configManager.config.masterSleepEnabled;
                syncPayload.activeStartHour      = configManager.config.activeStartHour;
                syncPayload.activeStartMin       = configManager.config.activeStartMin;
                syncPayload.activeEndHour        = configManager.config.activeEndHour;
                syncPayload.activeEndMin         = configManager.config.activeEndMin;
                syncPayload.normalIntervalSec    = configManager.config.tankNormalIntervalSec;
                syncPayload.fastIntervalSec      = configManager.config.tankFastIntervalSec;
                syncPayload.fastThresholdPct     = configManager.config.tankFastThresholdPct;
                syncPayload.isBoreholeRunning    = stateBorehole;
                syncPayload.syncId               = localTank.messageId;

                uint8_t bcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                esp_now_send(bcastMac, (uint8_t *)&syncPayload, sizeof(syncPayload));
            }
            else if (nodeId == NODE_SOLAR_PANEL && len >= sizeof(SolarNodePayload)) {
                {
                    StateLock lock;
                    memcpy(&solarData, incomingData, sizeof(SolarNodePayload));
                    lastNode4Time    = millis();
                    lastNode4TimeStr = nowTime;
                }
            } else {
                Serial.printf("⚠️ [Master] Recv Unknown/Size Mismatch: NodeId=%d, Len=%d\n", nodeId, len);
            }
        }
    }


    static void init(ConfigManager &configManager) {
        WiFi.onEvent(onWiFiEvent);
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("SmartWater-Master", "12345678");
        apStartTime = millis();
        apActive = true;
        Serial.print("Master AP Started (1 Minute Active). IP: ");
        Serial.println(WiFi.softAPIP());

        if (strlen(configManager.config.wifiSSID) > 0) {
            Serial.printf("Connecting to saved Wi-Fi: %s\n", configManager.config.wifiSSID);
            WiFi.begin(configManager.config.wifiSSID, configManager.config.wifiPassword);
        }

        if (MDNS.begin("smartwater")) {
            Serial.println("mDNS responder started: http://smartwater.local");
        }

        if (esp_now_init() == ESP_OK) {
            esp_now_register_recv_cb(onDataReceived);

            uint8_t bcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, bcastMac, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            esp_now_add_peer(&peerInfo);

            Serial.println("ESP-NOW initialized with broadcast peer.");
        }
    }

    static void handleLoop(ConfigManager &configManager) {
        // จัดการ Timeout ของ AP (เปิดแค่ 1 นาทีตามค่าเดิม หากไม่มีไคลเอนต์เชื่อมต่ออยู่)
        if (apActive) {
            int clientCount = WiFi.softAPgetStationNum();
            if (clientCount > 0) {
                apStartTime = millis();
            } else if (millis() - apStartTime >= AP_TIMEOUT_MS) {
                WiFi.softAPdisconnect(false);
                apActive = false;
                Serial.println("🔒 AP Timeout (No clients connected). AP broadcasting disabled.");
            }
        }

        // จัดการซิงค์เวลาเป็นระยะทุกๆ 1 ชั่วโมง
        if (WiFi.status() == WL_CONNECTED) {
            if (!ntpSynced || millis() - lastNtpAttempt >= 3600000UL) {
                lastNtpAttempt = millis();
                syncNtpTime();
            }
        }
    }
};

#endif // SYSTEM_NETWORK_H
