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
            String nowTime = getCurrentTimeString();

            StateLock lock;
            if (nodeId == NODE_WAVE_POOL && len >= sizeof(PoolNodePayload)) {
                memcpy(&pool1Data, incomingData, sizeof(PoolNodePayload));
                lastNode1Time = millis();
                lastNode1TimeStr = nowTime;
                Serial.printf("📥 [Master] Recv Node 1 (Wave Pool @ %s): Low=%s, Bat=%.2fV\n",
                              nowTime.c_str(), pool1Data.waterLow ? "YES" : "NO", pool1Data.batteryVoltage);
            }
            else if (nodeId == NODE_PLAY_POOL && len >= sizeof(PoolNodePayload)) {
                memcpy(&pool2Data, incomingData, sizeof(PoolNodePayload));
                lastNode2Time = millis();
                lastNode2TimeStr = nowTime;
                Serial.printf("📥 [Master] Recv Node 2 (Play Pool @ %s): Low=%s, Bat=%.2fV\n",
                              nowTime.c_str(), pool2Data.waterLow ? "YES" : "NO", pool2Data.batteryVoltage);
            }
            else if (nodeId == NODE_WATER_TANK && len >= sizeof(TankNodePayload)) {
                memcpy(&tankData, incomingData, sizeof(TankNodePayload));
                lastNode3Time = millis();
                lastNode3TimeStr = nowTime;

                // 🟢 Auto-Recovery: หากเคยติด Alarm เซ็นเซอร์หลุด (E3) เมื่อได้รับข้อมูลแล้วให้ปลดล็อกทันที
                if (currentError == ERR_NODE_LOST) {
                    currentError = ERR_NONE;
                    Serial.println("🟢 [ESP-NOW Recv Node 3] Auto-cleared Alarm E3 (Sensor Reconnected)!");
                }

                // 💧 Master ส่วนกลางคำนวณระดับน้ำเป็น % จากระยะ cm ที่ Node 3 ส่งมา
                float emptyCm = configManager.config.tankEmptyCm;
                float fullCm  = configManager.config.tankFullCm;
                if (tankData.distanceCm > 0.0f && emptyCm > fullCm) {
                    if (tankData.distanceCm >= emptyCm) {
                        tankData.waterLevelPercent = 0.0f;
                    } else if (tankData.distanceCm <= fullCm) {
                        tankData.waterLevelPercent = 100.0f;
                    } else {
                        tankData.waterLevelPercent = ((emptyCm - tankData.distanceCm) / (emptyCm - fullCm)) * 100.0f;
                    }
                } else if (tankData.distanceCm <= 0.0f) {
                    // เซ็นเซอร์อ่านไม่สำเร็จ (No Echo / Read Error)
                    tankData.waterLevelPercent = 0.0f;
                }

                Serial.printf("📥 [Master] Recv Node 3 @ %s: Raw Dist=%.1fcm -> Master Calc Level=%.1f%% (Empty:%.0f/Full:%.0f), Float=%s, Bat=%.2fV\n",
                              nowTime.c_str(), tankData.distanceCm, tankData.waterLevelPercent,
                              emptyCm, fullCm,
                              tankData.floatBackupActive ? "ACTIVE" : "NORMAL",
                              tankData.batteryVoltage);

                // Fail-safe: หากสวิตช์ลูกลอยแตะระดับตัด ให้ตัดปั๊มบาดาลทันที (ไม่ใช้ beep delay ใน WiFi ISR callback)
                if (tankData.floatBackupActive && stateBorehole) {
                    stateBorehole = false;
                    HardwareController::setRelay(RELAY_BOREHOLE, false);
                }
            }
            else if (nodeId == NODE_SOLAR_PANEL && len >= sizeof(SolarNodePayload)) {
                memcpy(&solarData, incomingData, sizeof(SolarNodePayload));
                lastNode4Time = millis();
                lastNode4TimeStr = nowTime;
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
