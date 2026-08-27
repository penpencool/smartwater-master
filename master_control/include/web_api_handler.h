#ifndef WEB_API_HANDLER_H
#define WEB_API_HANDLER_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include "system_pins.h"
#include "system_state.h"
#include "config_manager.h"
#include "hardware_controller.h"
#include "system_network.h"
#include "web_dashboard.h"

class WebApiHandler {
public:
    static void setupRoutes(WebServer &server, ConfigManager &configManager) {
        // 1. Dashboard Web Page (Modular HTML + CSS + JS)
        server.on("/", HTTP_GET, [&server]() {
            sendDashboardPage(server);
        });

        // 2. Real-time Status API
        server.on("/api/status", HTTP_GET, [&server, &configManager]() {
            // ✅ Fix #1: Copy-then-Release — copy shared state ภายใต้ lock แล้ว release ทันที
            // จากนั้น serialize JSON + server.send() โดยไม่ถือ lock (ป้องกัน Core 1 blocked)
            String _activeTaskName;
            int _errorCode;
            bool _isNode1Online, _node1WaterLow; float _node1Battery; unsigned long _lastNode1Sec; String _lastNode1TimeStr;
            bool _isNode2Online, _node2WaterLow; float _node2Battery; unsigned long _lastNode2Sec; String _lastNode2TimeStr;
            bool _isNode3Online; float _tankLevel, _tankDist; bool _tankFloat; float _node3Battery; unsigned long _lastNode3Sec; String _lastNode3TimeStr;
            bool _isNode4Online; unsigned long _lastNode4Sec; String _lastNode4TimeStr;
            unsigned long _nodeOfflineTimeoutMs;
            bool _pumpBorehole, _pumpFilter, _mainA, _mainB, _sv1, _sv2, _sv3, _sv4;
            uint8_t _currentGardenZone, _currentPoolTaskZone;
            unsigned long _gardenTaskStartTime, _gardenTaskDurationMs, _poolTaskStartTime, _poolTaskDurationMs;
            float _solarVolt, _solarWatt, _solarLux;
            float _tankEmptyCm, _tankFullCm;
            float _tankLowTrigger, _tankFullStop;
            bool _autoBorehole;
            float _gZ1Start, _gZ1Stop, _gZ2Start, _gZ2Stop;
            float _pWStart, _pWStop, _pPStart, _pPStop;
            bool _autoPoolWave, _autoPoolPlay; uint8_t _poolModeWave, _poolModePlay;
            uint8_t _poolWaveDelay, _poolWaveDur, _poolPlayDelay, _poolPlayDur;
            uint8_t _schedCount;
            ScheduleSlot _schedules[MAX_SCHEDULE_SLOTS];
            uint8_t _taskQueueCount;
            TaskQueueItem _taskQueue[MAX_QUEUE_SIZE];
            bool _ntpSynced;
            bool _apActive; int _apClients; unsigned long _apStartTime;
            char _githubRepo[64];
            char _wifiSSID[64];
            bool _masterSleep; uint8_t _activeStartH, _activeStartM, _activeEndH, _activeEndM;
            uint16_t _tNormInt, _tFastInt; float _tFastPct;

            unsigned long nowMs;
            {
                StateLock lock;
                nowMs = millis();
                _nodeOfflineTimeoutMs = (configManager.config.nodeOfflineTimeoutMin > 0) ? ((unsigned long)configManager.config.nodeOfflineTimeoutMin * 60000UL) : 120000UL;
                _activeTaskName  = activeTaskName;
                _errorCode       = (int)currentError;
                _isNode1Online   = (lastNode1Time > 0) && (nowMs - lastNode1Time <= _nodeOfflineTimeoutMs);
                _node1WaterLow   = _isNode1Online ? pool1Data.waterLow : false;
                _node1Battery    = _isNode1Online ? pool1Data.batteryVoltage : 0.0f;
                _lastNode1Sec    = (lastNode1Time > 0) ? ((nowMs - lastNode1Time) / 1000) : 9999;
                _lastNode1TimeStr = lastNode1TimeStr;
                _isNode2Online   = (lastNode2Time > 0) && (nowMs - lastNode2Time <= _nodeOfflineTimeoutMs);
                _node2WaterLow   = _isNode2Online ? pool2Data.waterLow : false;
                _node2Battery    = _isNode2Online ? pool2Data.batteryVoltage : 0.0f;
                _lastNode2Sec    = (lastNode2Time > 0) ? ((nowMs - lastNode2Time) / 1000) : 9999;
                _lastNode2TimeStr = lastNode2TimeStr;
                _isNode3Online   = (lastNode3Time > 0) && (nowMs - lastNode3Time <= _nodeOfflineTimeoutMs);
                _tankLevel       = _isNode3Online ? tankData.waterLevelPercent : 0.0f;
                _tankDist        = _isNode3Online ? tankData.distanceCm : -1.0f;
                _tankFloat       = _isNode3Online ? tankData.floatBackupActive : false;
                _node3Battery    = _isNode3Online ? tankData.batteryVoltage : 0.0f;
                _lastNode3Sec    = (lastNode3Time > 0) ? ((nowMs - lastNode3Time) / 1000) : 9999;
                _lastNode3TimeStr = lastNode3TimeStr;
                _isNode4Online   = (lastNode4Time > 0) && (nowMs - lastNode4Time <= _nodeOfflineTimeoutMs);
                _lastNode4Sec    = (lastNode4Time > 0) ? ((nowMs - lastNode4Time) / 1000) : 9999;
                _lastNode4TimeStr = lastNode4TimeStr;
                _pumpBorehole    = stateBorehole;
                _pumpFilter      = stateFilterPump;
                _mainA = stateMainA; _mainB = stateMainB;
                _sv1 = stateSV1; _sv2 = stateSV2; _sv3 = stateSV3; _sv4 = stateSV4;
                _currentGardenZone   = currentGardenZone;
                _currentPoolTaskZone = currentPoolTaskZone;
                _gardenTaskStartTime = gardenTaskStartTime;
                _gardenTaskDurationMs = gardenTaskDurationMs;
                _poolTaskStartTime   = poolTaskStartTime;
                _poolTaskDurationMs  = poolTaskDurationMs;
                _solarVolt = solarData.voltageDC;
                _solarWatt = solarData.powerWatt;
                _solarLux  = solarData.lightLux;
                _tankEmptyCm = (tankData.currentEmptyCm > 0) ? tankData.currentEmptyCm : configManager.config.tankEmptyCm;
                _tankFullCm  = (tankData.currentFullCm  > 0) ? tankData.currentFullCm  : configManager.config.tankFullCm;
                _autoBorehole    = configManager.config.autoBoreholeEnabled;
                _tankLowTrigger  = configManager.config.tankLowTrigger;
                _tankFullStop    = configManager.config.tankFullStop;
                _gZ1Start = configManager.config.gardenZ1StartLevel; _gZ1Stop = configManager.config.gardenZ1StopLevel;
                _gZ2Start = configManager.config.gardenZ2StartLevel; _gZ2Stop = configManager.config.gardenZ2StopLevel;
                _pWStart = configManager.config.poolWaveStartLevel; _pWStop = configManager.config.poolWaveStopLevel;
                _pPStart = configManager.config.poolPlayStartLevel; _pPStop = configManager.config.poolPlayStopLevel;
                _autoPoolWave = configManager.config.autoPoolWaveEnabled;
                _poolModeWave = configManager.config.poolModeWave;
                _poolWaveDelay = configManager.config.poolWaveDelayMin;
                _poolWaveDur   = configManager.config.poolWaveDurationMin;
                _autoPoolPlay = configManager.config.autoPoolPlayEnabled;
                _poolModePlay = configManager.config.poolModePlay;
                _poolPlayDelay = configManager.config.poolPlayDelayMin;
                _poolPlayDur   = configManager.config.poolPlayDurationMin;
                _schedCount = configManager.config.scheduleCount;
                for (uint8_t i = 0; i < _schedCount && i < MAX_SCHEDULE_SLOTS; i++) _schedules[i] = configManager.config.schedules[i];
                _taskQueueCount = taskQueueCount;
                for (uint8_t i = 0; i < _taskQueueCount && i < MAX_QUEUE_SIZE; i++) _taskQueue[i] = taskQueue[i];
                _ntpSynced  = ntpSynced;
                _apActive   = SystemNetwork::apActive;
                _apClients  = WiFi.softAPgetStationNum();
                _apStartTime = SystemNetwork::apStartTime;
                strncpy(_githubRepo, configManager.config.githubRepo, sizeof(_githubRepo)-1);
                _githubRepo[sizeof(_githubRepo)-1] = '\0';
                strncpy(_wifiSSID, configManager.config.wifiSSID, sizeof(_wifiSSID)-1);
                _wifiSSID[sizeof(_wifiSSID)-1] = '\0';

                // Power & Tank Sampling Config
                _masterSleep  = configManager.config.masterSleepEnabled;
                _activeStartH = configManager.config.activeStartHour;
                _activeStartM = configManager.config.activeStartMin;
                _activeEndH   = configManager.config.activeEndHour;
                _activeEndM   = configManager.config.activeEndMin;
                _tNormInt     = configManager.config.tankNormalIntervalSec;
                _tFastInt     = configManager.config.tankFastIntervalSec;
                _tFastPct     = configManager.config.tankFastThresholdPct;
            } // ← Release StateLock ทันทีหลัง copy

            // Build JSON จาก local copies (ไม่ต้องถือ lock)
            DynamicJsonDocument doc(3072);

            doc["activeTask"] = _activeTaskName;
            doc["errorCode"] = _errorCode;

            doc["node1Online"] = _isNode1Online;
            doc["node1WaterLow"] = _node1WaterLow;
            doc["node1Battery"] = _node1Battery;
            doc["lastNode1Sec"] = _lastNode1Sec;
            doc["lastNode1Time"] = _lastNode1TimeStr;

            doc["node2Online"] = _isNode2Online;
            doc["node2WaterLow"] = _node2WaterLow;
            doc["node2Battery"] = _node2Battery;
            doc["lastNode2Sec"] = _lastNode2Sec;
            doc["lastNode2Time"] = _lastNode2TimeStr;

            doc["node3Online"] = _isNode3Online;
            doc["lastNode3Sec"] = _lastNode3Sec;
            doc["lastNode3Time"] = _lastNode3TimeStr;
            doc["tankLevel"] = _tankLevel;
            doc["tankDist"] = _tankDist;
            doc["tankFloat"] = _tankFloat;
            doc["node3Battery"] = _node3Battery;

            doc["node4Online"] = _isNode4Online;
            doc["lastNode4Sec"] = _lastNode4Sec;
            doc["lastNode4Time"] = _lastNode4TimeStr;

            doc["nodeOfflineTimeoutMin"] = (unsigned long)(_nodeOfflineTimeoutMs / 60000UL);

            // Relays Status (All 8 Channels)
            doc["pumpBorehole"] = _pumpBorehole;
            doc["pumpFilter"] = _pumpFilter;
            doc["mainA"] = _mainA;
            doc["mainB"] = _mainB;
            doc["sv1"] = _sv1;
            doc["sv2"] = _sv2;
            doc["sv3"] = _sv3;
            doc["sv4"] = _sv4;
            doc["currentGardenZone"] = _currentGardenZone;
            doc["currentPoolTaskZone"] = _currentPoolTaskZone;

            // Task Remaining Seconds Countdown
            int taskRemainingSec = 0;
            if (_currentGardenZone > 0 && _gardenTaskDurationMs > 0) {
                unsigned long elapsed = nowMs - _gardenTaskStartTime;
                taskRemainingSec = (elapsed < _gardenTaskDurationMs) ? ((_gardenTaskDurationMs - elapsed) / 1000) : 0;
            } else if (_currentPoolTaskZone > 0 && _poolTaskDurationMs > 0) {
                unsigned long elapsed = nowMs - _poolTaskStartTime;
                taskRemainingSec = (elapsed < _poolTaskDurationMs) ? ((_poolTaskDurationMs - elapsed) / 1000) : 0;
            }
            doc["taskRemainingSec"] = taskRemainingSec;

            // Sensors & Solar
            doc["flowActive"] = (digitalRead(PIN_FLOW_SWITCH) == LOW);
            doc["solarVolt"] = _solarVolt;
            doc["solarWatt"] = _solarWatt;
            doc["solarLux"] = _solarLux;

            // Tank NVS Calibration Params
            doc["tankEmptyCm"] = _tankEmptyCm;
            doc["tankFullCm"] = _tankFullCm;

            // Queue Status & Items
            doc["queueCount"] = _taskQueueCount;
            JsonArray queueArr = doc.createNestedArray("queue");
            for (uint8_t i = 0; i < _taskQueueCount; i++) {
                JsonObject qObj = queueArr.createNestedObject();
                qObj["type"] = (uint8_t)_taskQueue[i].type;
                qObj["name"] = _taskQueue[i].taskName;
                qObj["zone"] = _taskQueue[i].targetZone;
                qObj["dur"] = _taskQueue[i].durationMin;
                int delaySec = 0;
                if (_taskQueue[i].executeAtMillis > nowMs) {
                    delaySec = (_taskQueue[i].executeAtMillis - nowMs) / 1000;
                }
                qObj["delaySec"] = delaySec;
                qObj["isAuto"] = (_taskQueue[i].type == TASK_GARDEN_AUTO || _taskQueue[i].type == TASK_POOL_AUTO);
            }

            // Pool Auto Top-up Configuration & State
            doc["autoPoolWave"] = _autoPoolWave;
            doc["poolModeWave"] = _poolModeWave;
            doc["poolWaveDelay"] = _poolWaveDelay;
            doc["poolWaveDur"] = _poolWaveDur;
            doc["autoPoolPlay"] = _autoPoolPlay;
            doc["poolModePlay"] = _poolModePlay;
            doc["poolPlayDelay"] = _poolPlayDelay;
            doc["poolPlayDur"] = _poolPlayDur;

            // 💧 Independent Zone Water Level Thresholds (%)
            doc["gZ1Start"] = _gZ1Start; doc["gZ1Stop"] = _gZ1Stop;
            doc["gZ2Start"] = _gZ2Start; doc["gZ2Stop"] = _gZ2Stop;
            doc["pWStart"] = _pWStart;   doc["pWStop"] = _pWStop;
            doc["pPStart"] = _pPStart;   doc["pPStop"] = _pPStop;

            // Auto Borehole Refill Thresholds
            doc["autoBorehole"] = _autoBorehole;
            doc["tankLowTrigger"] = _tankLowTrigger;
            doc["tankFullStop"] = _tankFullStop;

            // Dynamic Garden Schedules
            doc["schedCount"] = _schedCount;
            JsonArray schedArr = doc.createNestedArray("schedules");
            for (uint8_t i = 0; i < _schedCount; i++) {
                JsonObject sObj = schedArr.createNestedObject();
                sObj["enabled"]   = _schedules[i].enabled;
                sObj["zone"]      = _schedules[i].zone;
                sObj["startHour"] = _schedules[i].startHour;
                sObj["startMin"]  = _schedules[i].startMin;
                sObj["endHour"]   = _schedules[i].endHour;
                sObj["endMin"]    = _schedules[i].endMin;
            }

            doc["currentTime"] = SystemNetwork::getCurrentTimeString();
            doc["ntpSynced"] = _ntpSynced;

            // Wi-Fi Status (ตรวจ WiFi status นอก lock ได้ เพราะ WiFi API thread-safe ใน ESP32)
            bool isWifiConnected = (WiFi.status() == WL_CONNECTED);
            doc["wifiConnected"] = isWifiConnected;
            doc["staIP"] = isWifiConnected ? WiFi.localIP().toString() : "";
            doc["wifiSSID"] = (strlen(_wifiSSID) > 0) ? String(_wifiSSID) : "";

            doc["apClients"] = _apClients;
            doc["apActive"] = _apActive;
            if (_apActive) {
                if (_apClients > 0) {
                    doc["apRemainingSec"] = -1;
                } else {
                    unsigned long elapsed = nowMs - _apStartTime;
                    doc["apRemainingSec"] = (elapsed < SystemNetwork::AP_TIMEOUT_MS) ? ((SystemNetwork::AP_TIMEOUT_MS - elapsed) / 1000) : 0;
                }
            } else {
                doc["apRemainingSec"] = 0;
            }

            // Power Management & Tank Sampling Info
            doc["masterSleep"] = _masterSleep;
            doc["actStartH"] = _activeStartH;
            doc["actStartM"] = _activeStartM;
            doc["actEndH"] = _activeEndH;
            doc["actEndM"] = _activeEndM;
            doc["tNormInt"] = _tNormInt;
            doc["tFastInt"] = _tFastInt;
            doc["tFastPct"] = _tFastPct;

            // Firmware & GitHub Info
            doc["firmwareVer"] = "v1.4.0";
            doc["githubRepo"] = String(_githubRepo);

            String response;
            serializeJson(doc, response);
            server.send(200, "application/json", response);
        });

        // 2b. Power & Sleep Schedule Config API
        server.on("/api/power_config", HTTP_POST, [&server, &configManager]() {
            StateLock lock;
            if (server.hasArg("masterSleep")) {
                configManager.config.masterSleepEnabled = (server.arg("masterSleep") == "1" || server.arg("masterSleep") == "true");
            }
            if (server.hasArg("actStartH")) {
                configManager.config.activeStartHour = server.arg("actStartH").toInt();
            }
            if (server.hasArg("actStartM")) {
                configManager.config.activeStartMin = server.arg("actStartM").toInt();
            }
            if (server.hasArg("actEndH")) {
                configManager.config.activeEndHour = server.arg("actEndH").toInt();
            }
            if (server.hasArg("actEndM")) {
                configManager.config.activeEndMin = server.arg("actEndM").toInt();
            }

            configManager.save();
            HardwareController::soundBeep(1, 100);

            Serial.printf("⚙️ Updated Power Schedule: MasterSleep=%s, Active Window: %02d:%02d - %02d:%02d\n",
                          configManager.config.masterSleepEnabled ? "ON" : "OFF",
                          configManager.config.activeStartHour, configManager.config.activeStartMin,
                          configManager.config.activeEndHour, configManager.config.activeEndMin);

            server.send(200, "application/json", "{\"msg\":\"✅ บันทึกช่วงเวลาทำงานและโหมดประหยัดพลังงานเรียบร้อย\"}");
        });

        // 2c. 2-Stage Tank Sampling Config API
        server.on("/api/tank_sampling_config", HTTP_POST, [&server, &configManager]() {
            StateLock lock;
            if (server.hasArg("tNormInt")) {
                configManager.config.tankNormalIntervalSec = server.arg("tNormInt").toInt();
                if (configManager.config.tankNormalIntervalSec < 1) configManager.config.tankNormalIntervalSec = 1;
            }
            if (server.hasArg("tFastInt")) {
                configManager.config.tankFastIntervalSec = server.arg("tFastInt").toInt();
                if (configManager.config.tankFastIntervalSec < 1) configManager.config.tankFastIntervalSec = 1;
            }
            if (server.hasArg("tFastPct")) {
                configManager.config.tankFastThresholdPct = server.arg("tFastPct").toFloat();
            }

            configManager.save();
            HardwareController::soundBeep(1, 100);

            Serial.printf("⚙️ Updated Tank Sampling: Normal=%ds, Fast=%ds (when >= %.1f%% or Borehole Active)\n",
                          configManager.config.tankNormalIntervalSec,
                          configManager.config.tankFastIntervalSec,
                          configManager.config.tankFastThresholdPct);

            server.send(200, "application/json", "{\"msg\":\"✅ บันทึกความถี่การส่งข้อมูลเซ็นเซอร์แทงค์น้ำเรียบร้อย\"}");
        });

        // 3. Pool Auto Top-Up & Water Threshold Config API
        server.on("/api/pool_config", HTTP_POST, [&server, &configManager]() {
            StateLock lock;
            if (server.hasArg("autoWave")) {
                configManager.config.autoPoolWaveEnabled = (server.arg("autoWave") == "1" || server.arg("autoWave") == "true");
            }
            if (server.hasArg("poolModeWave")) {
                configManager.config.poolModeWave = server.arg("poolModeWave").toInt();
            }
            if (server.hasArg("waveDelay")) {
                configManager.config.poolWaveDelayMin = server.arg("waveDelay").toInt();
            }
            if (server.hasArg("waveDur")) {
                configManager.config.poolWaveDurationMin = server.arg("waveDur").toInt();
            }
            if (server.hasArg("pWStart")) {
                configManager.config.poolWaveStartLevel = server.arg("pWStart").toFloat();
            }
            if (server.hasArg("pWStop")) {
                configManager.config.poolWaveStopLevel = server.arg("pWStop").toFloat();
            }

            if (server.hasArg("autoPlay")) {
                configManager.config.autoPoolPlayEnabled = (server.arg("autoPlay") == "1" || server.arg("autoPlay") == "true");
            }
            if (server.hasArg("poolModePlay")) {
                configManager.config.poolModePlay = server.arg("poolModePlay").toInt();
            }
            if (server.hasArg("playDelay")) {
                configManager.config.poolPlayDelayMin = server.arg("playDelay").toInt();
            }
            if (server.hasArg("playDur")) {
                configManager.config.poolPlayDurationMin = server.arg("playDur").toInt();
            }
            if (server.hasArg("pPStart")) {
                configManager.config.poolPlayStartLevel = server.arg("pPStart").toFloat();
            }
            if (server.hasArg("pPStop")) {
                configManager.config.poolPlayStopLevel = server.arg("pPStop").toFloat();
            }

            configManager.save();
            HardwareController::soundBeep(1, 100);

            Serial.printf("⚙️ Updated Pool Config: Wave[Auto=%d, Mode=%d, Delay=%dm, Dur=%dm, Start=%.1f%%, Stop=%.1f%%], Play[Auto=%d, Mode=%d, Delay=%dm, Dur=%dm, Start=%.1f%%, Stop=%.1f%%]\n",
                          configManager.config.autoPoolWaveEnabled, configManager.config.poolModeWave, configManager.config.poolWaveDelayMin, configManager.config.poolWaveDurationMin,
                          configManager.config.poolWaveStartLevel, configManager.config.poolWaveStopLevel,
                          configManager.config.autoPoolPlayEnabled, configManager.config.poolModePlay, configManager.config.poolPlayDelayMin, configManager.config.poolPlayDurationMin,
                          configManager.config.poolPlayStartLevel, configManager.config.poolPlayStopLevel);

            server.send(200, "application/json", "{\"msg\":\"✅ บันทึกการตั้งค่าระดับสระว่ายน้ำและเกณฑ์แทงค์น้ำเรียบร้อย\"}");
        });

        // 3b. Garden Water Thresholds Config API
        server.on("/api/garden_thresholds", HTTP_POST, [&server, &configManager]() {
            StateLock lock;
            if (server.hasArg("gZ1Start")) {
                configManager.config.gardenZ1StartLevel = server.arg("gZ1Start").toFloat();
            }
            if (server.hasArg("gZ1Stop")) {
                configManager.config.gardenZ1StopLevel = server.arg("gZ1Stop").toFloat();
            }
            if (server.hasArg("gZ2Start")) {
                configManager.config.gardenZ2StartLevel = server.arg("gZ2Start").toFloat();
            }
            if (server.hasArg("gZ2Stop")) {
                configManager.config.gardenZ2StopLevel = server.arg("gZ2Stop").toFloat();
            }

            configManager.save();
            HardwareController::soundBeep(1, 100);

            Serial.printf("⚙️ Updated Garden Thresholds: Zone1[Start=%.1f%%, Stop=%.1f%%], Zone2[Start=%.1f%%, Stop=%.1f%%]\n",
                          configManager.config.gardenZ1StartLevel, configManager.config.gardenZ1StopLevel,
                          configManager.config.gardenZ2StartLevel, configManager.config.gardenZ2StopLevel);

            server.send(200, "application/json", "{\"msg\":\"✅ บันทึกเกณฑ์ระดับน้ำรดน้ำต้นไม้เรียบร้อย\"}");
        });

        // 4. Auto Borehole Config API
        server.on("/api/borehole_config", HTTP_POST, [&server, &configManager]() {
            StateLock lock;
            if (server.hasArg("auto")) {
                configManager.config.autoBoreholeEnabled = (server.arg("auto") == "1" || server.arg("auto") == "true");
            }
            if (server.hasArg("lowTrigger")) {
                configManager.config.tankLowTrigger = server.arg("lowTrigger").toFloat();
            }
            if (server.hasArg("fullStop")) {
                configManager.config.tankFullStop = server.arg("fullStop").toFloat();
            }

            configManager.save();
            HardwareController::soundBeep(1, 100);

            Serial.printf("⚙️ Updated Borehole Config: Auto=%s, Start<%.1f%%, Stop>=%.1f%%\n",
                          configManager.config.autoBoreholeEnabled ? "ON" : "OFF",
                          configManager.config.tankLowTrigger,
                          configManager.config.tankFullStop);

            server.send(200, "application/json", "{\"msg\":\"✅ บันทึกการตั้งค่าปั๊มบาดาลอัตโนมัติเรียบร้อย\"}");
        });

        // 4b. Garden Schedule Config API
        server.on("/api/garden_config", HTTP_POST, [&server, &configManager]() {
            if (server.hasArg("plain")) {
                StaticJsonDocument<1024> doc;
                DeserializationError err = deserializeJson(doc, server.arg("plain"));
                if (!err) {
                    StateLock lock;
                    JsonArray arr = doc["schedules"].as<JsonArray>();
                    uint8_t count = arr.size();
                    if (count > MAX_SCHEDULE_SLOTS) count = MAX_SCHEDULE_SLOTS;
                    configManager.config.scheduleCount = count;

                    for (uint8_t i = 0; i < count; i++) {
                        configManager.config.schedules[i].enabled = arr[i]["enabled"] | false;
                        configManager.config.schedules[i].zone = arr[i]["zone"] | 1;
                        configManager.config.schedules[i].startHour = arr[i]["startHour"] | 0;
                        configManager.config.schedules[i].startMin = arr[i]["startMin"] | 0;
                        configManager.config.schedules[i].endHour = arr[i]["endHour"] | 0;
                        configManager.config.schedules[i].endMin = arr[i]["endMin"] | 0;
                    }

                    configManager.save();
                    HardwareController::soundBeep(1, 100);
                    Serial.printf("⚙️ Updated %d Dynamic Garden Schedules via JSON.\n", count);
                    server.send(200, "application/json", "{\"msg\":\"✅ บันทึกตารางเวลารดน้ำต้นไม้เรียบร้อย\"}");
                    return;
                }
            }
            server.send(400, "application/json", "{\"error\":\"Invalid schedule payload\"}");
        });

        // 5. Tank Calibration API
        server.on("/api/tank_calibrate", HTTP_POST, [&server, &configManager]() {
            if (!server.hasArg("empty") || !server.hasArg("full")) {
                server.send(400, "application/json", "{\"error\":\"Missing params\"}");
                return;
            }

            float emptyVal = server.arg("empty").toFloat();
            float fullVal = server.arg("full").toFloat();

            if (emptyVal <= fullVal) {
                server.send(400, "application/json", "{\"error\":\"Empty cm must be greater than Full cm\"}");
                return;
            }

            // บันทึกลง Master NVS และคำนวณระดับน้ำ % ทันที
            {
                StateLock lock;
                configManager.config.tankEmptyCm = emptyVal;
                configManager.config.tankFullCm = fullVal;
                configManager.save();

                // คำนวณระดับน้ำ % ใหม่ทันทีจากระยะ cm ล่าสุด
                if (tankData.distanceCm > 0.0f && emptyVal > fullVal) {
                    if (tankData.distanceCm >= emptyVal) {
                        tankData.waterLevelPercent = 0.0f;
                    } else if (tankData.distanceCm <= fullVal) {
                        tankData.waterLevelPercent = 100.0f;
                    } else {
                        tankData.waterLevelPercent = ((emptyVal - tankData.distanceCm) / (emptyVal - fullVal)) * 100.0f;
                    }
                }
            }

            Serial.printf("💾 [Master Central] Tank Calibration saved: Empty=%.1f cm, Full=%.1f cm (Recalculated Level: %.1f%%)\n",
                          emptyVal, fullVal, tankData.waterLevelPercent);

            HardwareController::soundBeep(1, 150);
            server.send(200, "application/json", "{\"msg\":\"✅ บันทึกการตั้งค่าระยะถังน้ำลง Master เรียบร้อย (ระดับน้ำ: " + String(tankData.waterLevelPercent, 1) + "%)\"}");
        });

        // 6. System Parameters Config API (Offline Timeout, etc.)
        server.on("/api/system_config", HTTP_POST, [&server, &configManager]() {
            StateLock lock;
            if (server.hasArg("nodeOffMin")) {
                configManager.config.nodeOfflineTimeoutMin = server.arg("nodeOffMin").toInt();
                if (configManager.config.nodeOfflineTimeoutMin < 1) configManager.config.nodeOfflineTimeoutMin = 1;
            }
            configManager.save();
            HardwareController::soundBeep(1, 100);
            Serial.printf("⚙️ Updated System Config: Node Offline Timeout = %d min\n", configManager.config.nodeOfflineTimeoutMin);
            server.send(200, "application/json", "{\"msg\":\"✅ บันทึกเวลาตรวจจับเซ็นเซอร์ออฟไลน์เรียบร้อย (" + String(configManager.config.nodeOfflineTimeoutMin) + " นาที)\"}");
        });

        // 7. Tank Level Simulator API (สำหรับทดสอบระบบ)
        server.on("/api/sim_tank", HTTP_POST, [&server]() {
            if (!server.hasArg("enable")) {
                server.send(400, "application/json", "{\"error\":\"Missing enable flag\"}");
                return;
            }

            StateLock lock;
            bool enable = (server.arg("enable") == "1" || server.arg("enable") == "true");
            if (enable) {
                float level = server.hasArg("level") ? server.arg("level").toFloat() : 80.0f;
                if (level < 0.0f) level = 0.0f;
                if (level > 100.0f) level = 100.0f;

                simTankActive = true;
                simTankLevel = level;
                tankData.waterLevelPercent = level;
                tankData.distanceCm = 180.0f - (level * 1.55f);
                tankData.batteryVoltage = 3.95f;
                tankData.floatBackupActive = (level >= 98.0f);
                lastNode3Time = millis();
                lastNode3TimeStr = "จำลอง (Simulated " + String(level, 1) + "%)";

                // หากมี Error E3 (Node Lost) ค้างอยู่ ให้เคลียร์อัตโนมัติเพื่อให้ทดสอบระบบได้ทันที
                if (currentError == ERR_NODE_LOST) {
                    currentError = ERR_NONE;
                }

                HardwareController::soundBeep(2, 80);
                Serial.printf("🧪 [SIMULATOR] Injected simulated tank level = %.1f%%\n", level);
                server.send(200, "application/json", "{\"msg\":\"🧪 เปิดโหมดจำลองระดับน้ำที่ " + String(level, 1) + "% เรียบร้อย (เคลียร์ Alarm ให้พร้อมทดสอบ)\"}");
            } else {
                simTankActive = false;
                simTankLevel = 0.0f;
                lastNode3Time = 0; // รีเซ็ตกลับไปรอค่าจริง
                lastNode3TimeStr = "";
                tankData.waterLevelPercent = 0.0f;
                HardwareController::soundBeep(1, 150);
                Serial.println("🧪 [SIMULATOR] Cancelled simulation. Reverted to real sensor.");
                server.send(200, "application/json", "{\"msg\":\"🔄 ยกเลิกการจำลอง กลับไปรับข้อมูลจากเซ็นเซอร์จริง Node 3\"}");
            }
        });

        // 8. Pool Status Simulator API (สำหรับทดสอบสระคลื่น และ สระเล่น)
        server.on("/api/sim_pool", HTTP_POST, [&server]() {
            if (!server.hasArg("pool") || !server.hasArg("enable")) {
                server.send(400, "application/json", "{\"error\":\"Missing parameters\"}");
                return;
            }

            StateLock lock;
            String pool = server.arg("pool");
            bool enable = (server.arg("enable") == "1" || server.arg("enable") == "true");
            bool waterLow = server.hasArg("low") ? (server.arg("low") == "1" || server.arg("low") == "true") : false;

            if (pool == "1" || pool == "wave") {
                simPool1Active = enable;
                simPool1Low = waterLow;
                if (enable) {
                    lastNode1Time = millis();
                    lastNode1TimeStr = "จำลอง (Simulated)";
                    pool1Data.waterLow = waterLow;
                    pool1Data.batteryVoltage = 3.92f;
                    HardwareController::soundBeep(2, 80);
                    Serial.printf("🧪 [SIMULATOR] Wave Pool (Node 1) simulated: WaterLow=%s\n", waterLow ? "YES (ต้องการเติม)" : "NO (เต็ม)");
                    server.send(200, "application/json", "{\"msg\":\"🧪 จำลองสถานะสระคลื่น (Node 1): " + String(waterLow ? "⚠️ น้ำลด (ต้องการเติม)" : "🟢 ปกติ (น้ำเต็ม)") + "\"}");
                } else {
                    lastNode1Time = 0;
                    lastNode1TimeStr = "";
                    pool1Data.waterLow = false;
                    HardwareController::soundBeep(1, 150);
                    Serial.println("🧪 [SIMULATOR] Wave Pool simulation cancelled.");
                    server.send(200, "application/json", "{\"msg\":\"🔄 ยกเลิกการจำลองสระคลื่น กลับไปรับข้อมูลจริงจาก Node 1\"}");
                }
            } else if (pool == "2" || pool == "play") {
                simPool2Active = enable;
                simPool2Low = waterLow;
                if (enable) {
                    lastNode2Time = millis();
                    lastNode2TimeStr = "จำลอง (Simulated)";
                    pool2Data.waterLow = waterLow;
                    pool2Data.batteryVoltage = 3.88f;
                    HardwareController::soundBeep(2, 80);
                    Serial.printf("🧪 [SIMULATOR] Play Pool (Node 2) simulated: WaterLow=%s\n", waterLow ? "YES (ต้องการเติม)" : "NO (เต็ม)");
                    server.send(200, "application/json", "{\"msg\":\"🧪 จำลองสถานะสระเล่น (Node 2): " + String(waterLow ? "⚠️ น้ำลด (ต้องการเติม)" : "🟢 ปกติ (น้ำเต็ม)") + "\"}");
                } else {
                    lastNode2Time = 0;
                    lastNode2TimeStr = "";
                    pool2Data.waterLow = false;
                    HardwareController::soundBeep(1, 150);
                    Serial.println("🧪 [SIMULATOR] Play Pool simulation cancelled.");
                    server.send(200, "application/json", "{\"msg\":\"🔄 ยกเลิกการจำลองสระเล่น กลับไปรับข้อมูลจริงจาก Node 2\"}");
                }
            } else {
                server.send(400, "application/json", "{\"error\":\"Unknown pool\"}");
            }
        });

        // 9. Save Wi-Fi Credentials API
        server.on("/api/wifi", HTTP_POST, [&server, &configManager]() {
            if (!server.hasArg("ssid")) {
                server.send(400, "application/json", "{\"error\":\"Missing SSID\"}");
                return;
            }

            String ssid = server.arg("ssid");
            String pass = server.hasArg("pass") ? server.arg("pass") : "";

            {
                StateLock lock;
                strncpy(configManager.config.wifiSSID, ssid.c_str(), sizeof(configManager.config.wifiSSID) - 1);
                configManager.config.wifiSSID[sizeof(configManager.config.wifiSSID) - 1] = '\0';
                strncpy(configManager.config.wifiPassword, pass.c_str(), sizeof(configManager.config.wifiPassword) - 1);
                configManager.config.wifiPassword[sizeof(configManager.config.wifiPassword) - 1] = '\0';
                configManager.save();
            }

            Serial.printf("Connecting to Wi-Fi SSID: %s\n", ssid.c_str());
            WiFi.disconnect(false);
            delay(100);
            WiFi.begin(configManager.config.wifiSSID, configManager.config.wifiPassword);

            server.send(200, "application/json", "{\"msg\":\"✅ บันทึก Wi-Fi สำเร็จ กำลังเชื่อมต่อเครือข่าย...\"}");
        });

        // 10. Manual Command API
        server.on("/api/command", HTTP_POST, [&server, &configManager]() {
            if (!server.hasArg("action")) {
                server.send(400, "application/json", "{\"error\":\"Missing action\"}");
                return;
            }

            StateLock lock;
            String action = server.arg("action");

            if (action == "pump_borehole_toggle" || action == "pump_borehole_on" || action == "pump_borehole_off") {
                if (action == "pump_borehole_on") {
                    if (stateFilterPump) {
                        server.send(200, "application/json", "{\"msg\":\"⚠️ ไม่สามารถเปิดบาดาลได้ขณะปั๊มดันน้ำทำงาน (Interlock)\"}");
                        return;
                    }
                    stateBorehole = true;
                } else if (action == "pump_borehole_off") {
                    stateBorehole = false;
                } else {
                    if (!stateBorehole && stateFilterPump) {
                        server.send(200, "application/json", "{\"msg\":\"⚠️ ไม่สามารถเปิดบาดาลได้ขณะปั๊มดันน้ำทำงาน (Interlock)\"}");
                        return;
                    }
                    stateBorehole = !stateBorehole;
                }

                HardwareController::setRelay(RELAY_BOREHOLE, stateBorehole);
                HardwareController::soundBeep(1, 100);
                server.send(200, "application/json", "{\"msg\":\"ปั๊มบาดาล: " + String(stateBorehole ? "เปิด (ON)" : "ปิด (OFF)") + "\", \"state\":" + (stateBorehole ? "true" : "false") + "}");
            }
            else if (action == "garden_zone1_start") {
                uint16_t dur = server.hasArg("dur") ? server.arg("dur").toInt() : 15;
                if (dur == 0) dur = 15;
                if (currentGardenZone == 0 && currentPoolTaskZone == 0 && !stateBorehole && currentError == ERR_NONE) {
                    if (HardwareController::startGardenZone(1, dur)) {
                        server.send(200, "application/json", "{\"msg\":\"🌱 เริ่มรดน้ำโซน 1 (SV3) จำนวน " + String(dur) + " นาที\"}");
                    } else {
                        String reason = "⚠️ ไม่สามารถเริ่มรดน้ำได้: ";
                        if (currentError == ERR_NODE_LOST) reason += "Node 3 เซ็นเซอร์แทงค์น้ำออฟไลน์ ไม่ทราบระดับน้ำเพื่อความปลอดภัย";
                        else if (currentError == ERR_TANK_DRY) reason += "น้ำในแทงค์ต่ำวิกฤต (<= 25%) ปั๊มไม่ทำงาน";
                        else reason += "ระบบอยู่ในสถานะ Alarm";
                        server.send(200, "application/json", "{\"msg\":\"" + reason + "\", \"error\":true}");
                    }
                } else if (currentError == ERR_NONE) {
                    // เข้ารอในคิว Priority 1
                    TaskQueueManager::enqueue(TASK_GARDEN_MANUAL, 1, dur, "รดน้ำโซน 1 (คำสั่งด่วน)");
                    HardwareController::soundBeep(2, 80);
                    server.send(200, "application/json", "{\"msg\":\"⏳ เพิ่ม 'รดน้ำโซน 1' เข้าคิวลำดับความสำคัญสูงสุดเรียบร้อย (ระบบจะรันทันทีเมื่องานปัจจุบันเสร็จ/หยุดปั๊มบาดาล)\"}");
                } else {
                    server.send(200, "application/json", "{\"msg\":\"⚠️ ระบบติดสถานะ Error Alarm (E" + String(currentError) + ") กรุณารีเซ็ต Alarm ก่อนสั่งงาน\", \"error\":true}");
                }
            }
            else if (action == "garden_zone2_start") {
                uint16_t dur = server.hasArg("dur") ? server.arg("dur").toInt() : 15;
                if (dur == 0) dur = 15;
                if (currentGardenZone == 0 && currentPoolTaskZone == 0 && !stateBorehole && currentError == ERR_NONE) {
                    if (HardwareController::startGardenZone(2, dur)) {
                        server.send(200, "application/json", "{\"msg\":\"🌱 เริ่มรดน้ำโซน 2 (SV4) จำนวน " + String(dur) + " นาที\"}");
                    } else {
                        String reason = "⚠️ ไม่สามารถเริ่มรดน้ำได้: ";
                        if (currentError == ERR_NODE_LOST) reason += "Node 3 เซ็นเซอร์แทงค์น้ำออฟไลน์ ไม่ทราบระดับน้ำเพื่อความปลอดภัย";
                        else if (currentError == ERR_TANK_DRY) reason += "น้ำในแทงค์ต่ำวิกฤต (<= 25%) ปั๊มไม่ทำงาน";
                        else reason += "ระบบอยู่ในสถานะ Alarm";
                        server.send(200, "application/json", "{\"msg\":\"" + reason + "\", \"error\":true}");
                    }
                } else if (currentError == ERR_NONE) {
                    // เข้ารอในคิว Priority 1
                    TaskQueueManager::enqueue(TASK_GARDEN_MANUAL, 2, dur, "รดน้ำโซน 2 (คำสั่งด่วน)");
                    HardwareController::soundBeep(2, 80);
                    server.send(200, "application/json", "{\"msg\":\"⏳ เพิ่ม 'รดน้ำโซน 2' เข้าคิวลำดับความสำคัญสูงสุดเรียบร้อย (ระบบจะรันทันทีเมื่องานปัจจุบันเสร็จ/หยุดปั๊มบาดาล)\"}");
                } else {
                    server.send(200, "application/json", "{\"msg\":\"⚠️ ระบบติดสถานะ Error Alarm (E" + String(currentError) + ") กรุณารีเซ็ต Alarm ก่อนสั่งงาน\", \"error\":true}");
                }
            }
            else if (action == "garden_stop") {
                HardwareController::stopGardenSprinkler();
                server.send(200, "application/json", "{\"msg\":\"⏹️ หยุดระบบรดน้ำต้นไม้เรียบร้อย\"}");
            }
            else if (action == "pool_wave_start") {
                uint16_t dur = server.hasArg("dur") ? server.arg("dur").toInt() : 15;
                if (dur == 0) dur = 15;

                // 🛡️ ป้องกันน้ำล้น: หากสระคลื่นน้ำเต็มอยู่แล้ว ไม่อนุญาตให้เริ่มหรือเข้าคิว
                if (!pool1Data.waterLow) {
                    HardwareController::soundBeep(2, 100);
                    server.send(200, "application/json", "{\"msg\":\"⚠️ ไม่สามารถเติมน้ำได้: สระคลื่นระดับน้ำเต็มอยู่แล้ว (ลูกลอยปกติ)\", \"error\":true}");
                    return;
                }

                if (currentGardenZone == 0 && currentPoolTaskZone == 0 && !stateBorehole && currentError == ERR_NONE) {
                    isManualPoolTask = true;
                    if (HardwareController::startPoolTopUp(1, dur)) {
                        server.send(200, "application/json", "{\"msg\":\"🌊 เริ่มเติมน้ำสระคลื่น (SV1) จำนวน " + String(dur) + " นาที\"}");
                    } else {
                        String reason = "⚠️ ไม่สามารถเริ่มเติมน้ำได้: ";
                        if (currentError == ERR_NODE_LOST) reason += "Node 3 เซ็นเซอร์แทงค์น้ำออฟไลน์ ไม่ทราบระดับน้ำเพื่อความปลอดภัย";
                        else if (currentError == ERR_TANK_DRY) reason += "น้ำในแทงค์ต่ำวิกฤต (<= 25%) ปั๊มไม่ทำงาน";
                        else reason += "ระบบอยู่ในสถานะ Alarm";
                        server.send(200, "application/json", "{\"msg\":\"" + reason + "\", \"error\":true}");
                    }
                } else if (currentError == ERR_NONE) {
                    // เข้ารอในคิว Priority 3
                    TaskQueueManager::enqueue(TASK_POOL_MANUAL, 1, dur, "เติมน้ำสระคลื่น (คำสั่งด่วน)");
                    HardwareController::soundBeep(2, 80);
                    server.send(200, "application/json", "{\"msg\":\"⏳ เพิ่ม 'เติมสระคลื่น' เข้าคิวเรียบร้อย (ระบบจะเริ่มทำงานอัตโนมัติเมื่อคิวว่าง)\"}");
                } else {
                    server.send(200, "application/json", "{\"msg\":\"⚠️ ระบบติดสถานะ Error Alarm (E" + String(currentError) + ") กรุณารีเซ็ต Alarm ก่อนสั่งงาน\", \"error\":true}");
                }
            }
            else if (action == "pool_play_start") {
                uint16_t dur = server.hasArg("dur") ? server.arg("dur").toInt() : 15;
                if (dur == 0) dur = 15;

                // 🛡️ ป้องกันน้ำล้น: หากสระเล่นน้ำเต็มอยู่แล้ว ไม่อนุญาตให้เริ่มหรือเข้าคิว
                if (!pool2Data.waterLow) {
                    HardwareController::soundBeep(2, 100);
                    server.send(200, "application/json", "{\"msg\":\"⚠️ ไม่สามารถเติมน้ำได้: สระเล่นระดับน้ำเต็มอยู่แล้ว (ลูกลอยปกติ)\", \"error\":true}");
                    return;
                }

                if (currentGardenZone == 0 && currentPoolTaskZone == 0 && !stateBorehole && currentError == ERR_NONE) {
                    isManualPoolTask = true;
                    if (HardwareController::startPoolTopUp(2, dur)) {
                        server.send(200, "application/json", "{\"msg\":\"🏊 เริ่มเติมน้ำสระเล่น (SV2) จำนวน " + String(dur) + " นาที\"}");
                    } else {
                        String reason = "⚠️ ไม่สามารถเริ่มเติมน้ำได้: ";
                        if (currentError == ERR_NODE_LOST) reason += "Node 3 เซ็นเซอร์แทงค์น้ำออฟไลน์ ไม่ทราบระดับน้ำเพื่อความปลอดภัย";
                        else if (currentError == ERR_TANK_DRY) reason += "น้ำในแทงค์ต่ำวิกฤต (<= 25%) ปั๊มไม่ทำงาน";
                        else reason += "ระบบอยู่ในสถานะ Alarm";
                        server.send(200, "application/json", "{\"msg\":\"" + reason + "\", \"error\":true}");
                    }
                } else if (currentError == ERR_NONE) {
                    // เข้ารอในคิว Priority 3
                    TaskQueueManager::enqueue(TASK_POOL_MANUAL, 2, dur, "เติมน้ำสระเล่น (คำสั่งด่วน)");
                    HardwareController::soundBeep(2, 80);
                    server.send(200, "application/json", "{\"msg\":\"⏳ เพิ่ม 'เติมสระเล่น' เข้าคิวเรียบร้อย (ระบบจะเริ่มทำงานอัตโนมัติเมื่อคิวว่าง)\"}");
                } else {
                    server.send(200, "application/json", "{\"msg\":\"⚠️ ระบบติดสถานะ Error Alarm (E" + String(currentError) + ") กรุณารีเซ็ต Alarm ก่อนสั่งงาน\", \"error\":true}");
                }
            }
            else if (action == "pool_stop") {
                isManualPoolTask = false;
                HardwareController::stopPoolTopUp();
                
                unsigned long recheckDelayMs = 5UL * 60000UL; // 5 นาที
                String msgSuffix = "";

                // คำนวณการเติมน้ำลงสระทั้งสองทันที: ถ้าน้ำลดให้เข้าคิวทันที พร้อมนับถอยหลัง 5 นาที
                if (pool1Data.waterLow && configManager.config.autoPoolWaveEnabled) {
                    poolLowStartTimeWave = millis();
                    TaskQueueManager::enqueue(TASK_POOL_AUTO, 1, configManager.config.poolWaveDurationMin, "เติมน้ำสระคลื่นอัตโนมัติ", recheckDelayMs);
                    msgSuffix += " [สระคลื่น: เข้าคิวรอ 5 นาที]";
                }
                if (pool2Data.waterLow && configManager.config.autoPoolPlayEnabled) {
                    poolLowStartTimePlay = millis();
                    TaskQueueManager::enqueue(TASK_POOL_AUTO, 2, configManager.config.poolPlayDurationMin, "เติมน้ำสระเล่นอัตโนมัติ", recheckDelayMs);
                    msgSuffix += " [สระเล่น: เข้าคิวรอ 5 นาที]";
                }

                server.send(200, "application/json", "{\"msg\":\"⏹️ หยุดการเติมน้ำสระเรียบร้อย" + msgSuffix + "\"}");
            }
            else if (action == "stop_all") {
                isManualPoolTask = false;
                HardwareController::stopAllOutputs();
                HardwareController::soundBeep(2, 100);
                Serial.println("⏹️ [USER] Normal Stop All outputs triggered.");

                unsigned long recheckDelayMs = 5UL * 60000UL; // 5 นาที
                String msgSuffix = "";

                // คำนวณการเติมน้ำลงสระทั้งสองทันที: ถ้าน้ำลดให้เข้าคิวทันที พร้อมนับถอยหลัง 5 นาที
                if (pool1Data.waterLow && configManager.config.autoPoolWaveEnabled) {
                    poolLowStartTimeWave = millis(); // เริ่มนับ delay ใหม่
                    TaskQueueManager::enqueue(TASK_POOL_AUTO, 1, configManager.config.poolWaveDurationMin, "เติมน้ำสระคลื่นอัตโนมัติ", recheckDelayMs);
                    msgSuffix += " [สระคลื่น: เข้าคิวรอ 5 นาที]";
                }
                if (pool2Data.waterLow && configManager.config.autoPoolPlayEnabled) {
                    poolLowStartTimePlay = millis(); // เริ่มนับ delay ใหม่
                    TaskQueueManager::enqueue(TASK_POOL_AUTO, 2, configManager.config.poolPlayDurationMin, "เติมน้ำสระเล่นอัตโนมัติ", recheckDelayMs);
                    msgSuffix += " [สระเล่น: เข้าคิวรอ 5 นาที]";
                }

                server.send(200, "application/json", "{\"msg\":\"⏹️ สั่งหยุดการทำงานทุกระบบเรียบร้อย" + msgSuffix + "\"}");
            }
            else if (action == "emergency_stop") {
                isManualPoolTask = false;
                HardwareController::stopAllOutputs();
                currentError = ERR_ESTOP;
                HardwareController::soundBeep(3, 200);
                server.send(200, "application/json", "{\"msg\":\"🚨 สั่ง EMERGENCY STOP เรียบร้อย ระบบตัดทุกโหลดและล็อก Alarm\"}");
            }
            else if (action == "reset_error") {
                currentError = ERR_NONE;
                HardwareController::soundBeep(1, 150);
                Serial.println("🟢 [ALARM RESET] Error cleared by User.");
                server.send(200, "application/json", "{\"msg\":\"🟢 ปลดล็อกและรีเซ็ตสถานะข้อผิดพลาดเรียบร้อย (ระบบกลับสู่ปกติ)\"}");
            }
            else {
                server.send(400, "application/json", "{\"error\":\"Unknown action\"}");
            }
        });

        // 11. Web OTA Firmware Update API (อัปเดตเฟิร์มแวร์ผ่านหน้าเว็บ)
        server.on("/api/update", HTTP_POST, [&server]() {
            bool success = !Update.hasError();
            server.sendHeader("Connection", "close");
            if (success) {
                server.send(200, "application/json", "{\"status\":\"OK\",\"msg\":\"✅ อัปเดตเฟิร์มแวร์สำเร็จ! กำลังรีบูตระบบ...\"}");
                delay(800);
                ESP.restart();
            } else {
                server.send(500, "application/json", "{\"status\":\"ERROR\",\"msg\":\"❌ อัปเดตเฟิร์มแวร์ล้มเหลว: " + String(Update.errorString()) + "\"}");
            }
        }, [&server]() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
                Serial.printf("⚡ [OTA] Starting Update: %s\n", upload.filename.c_str());
                // 🛡️ ตัดการทำงานของปั๊มทั้งหมดทันทีก่อนเขียน Flash เพื่อความปลอดภัยสูงสุด
                HardwareController::stopAllOutputs();
                if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                    Update.printError(Serial);
                }
            } else if (upload.status == UPLOAD_FILE_WRITE) {
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                    Update.printError(Serial);
                }
            } else if (upload.status == UPLOAD_FILE_END) {
                if (Update.end(true)) {
                    Serial.printf("✅ [OTA] Update Success: %u bytes written\n", upload.totalSize);
                } else {
                    Update.printError(Serial);
                }
            } else if (upload.status == UPLOAD_FILE_ABORTED) {
                Update.end();
                Serial.println("❌ [OTA] Update Aborted!");
            }
        });

        // 12. GitHub Repository Config API
        server.on("/api/github_config", HTTP_POST, [&server, &configManager]() {
            if (server.hasArg("repo")) {
                String repo = server.arg("repo");
                repo.trim();
                strncpy(configManager.config.githubRepo, repo.c_str(), sizeof(configManager.config.githubRepo) - 1);
                configManager.config.githubRepo[sizeof(configManager.config.githubRepo) - 1] = '\0';
                configManager.save();
                server.send(200, "application/json", "{\"msg\":\"✅ บันทึก GitHub Repository เรียบร้อย: " + repo + "\"}");
            } else {
                server.send(400, "application/json", "{\"error\":\"Missing repo parameter\"}");
            }
        });

        // 13. Check Latest Release on GitHub API
        server.on("/api/github_check", HTTP_GET, [&server, &configManager]() {
            if (WiFi.status() != WL_CONNECTED) {
                server.send(200, "application/json", "{\"online\":false, \"error\":\"ESP32 ยังไม่ได้เชื่อมต่ออินเทอร์เน็ต (Wi-Fi STA)\"}");
                return;
            }

            String repo = String(configManager.config.githubRepo);
            if (repo.length() == 0) repo = "penpencool/smartwater-master";

            String apiUrl = "https://api.github.com/repos/" + repo + "/releases/latest";
            
            WiFiClientSecure client;
            client.setInsecure();
            client.setTimeout(8);  // ✅ Fix #3: ลด timeout 15s → 8s ป้องกัน Core 0 blocked นาน

            HTTPClient http;
            http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
            http.setTimeout(8000);  // ✅ Fix #3
            http.begin(client, apiUrl);
            http.setUserAgent("ESP32-SmartWater-Master-OTA");
            http.addHeader("Accept", "application/vnd.github.v3+json");

            int httpCode = http.GET();
            if (httpCode == 200) {
                String payload = http.getString();
                http.end();

                DynamicJsonDocument doc(3072);  // ✅ Fix: ลด Heap alloc จาก 4096 → 3072
                DeserializationError err = deserializeJson(doc, payload);
                if (!err) {
                    StaticJsonDocument<1024> resDoc;
                    resDoc["online"] = true;
                    resDoc["tag"] = doc["tag_name"] | "v1.0.0";
                    resDoc["name"] = doc["name"] | doc["tag_name"] | "Latest Release";
                    resDoc["published"] = doc["published_at"] | "";
                    String body = doc["body"] | "";
                    if (body.length() > 250) body = body.substring(0, 250) + "...";
                    resDoc["body"] = body;
                    resDoc["downloadUrl"] = "https://github.com/" + repo + "/releases/latest/download/firmware.bin";
                    resDoc["repo"] = repo;

                    String res;
                    serializeJson(resDoc, res);
                    server.send(200, "application/json", res);
                    return;
                } else {
                    server.send(200, "application/json", "{\"online\":true, \"error\":\"แปลงข้อมูล JSON จาก GitHub ไม่สำเร็จ\"}");
                    return;
                }
            } else if (httpCode == 404) {
                http.end();
                server.send(200, "application/json", "{\"online\":true, \"error\":\"ไม่พบ Release ใน Repository (" + repo + ") กรุณาสร้าง Release แรกบน GitHub หรือตรวจชื่อ Repo\"}");
                return;
            } else {
                http.end();
                server.send(200, "application/json", "{\"online\":true, \"error\":\"เชื่อมต่อ GitHub API ไม่สำเร็จ (HTTP " + String(httpCode) + ")\"}");
                return;
            }
        });

        // 14. GitHub Direct Cloud OTA Update API
        server.on("/api/github_update", HTTP_POST, [&server, &configManager]() {
            if (WiFi.status() != WL_CONNECTED) {
                server.send(400, "application/json", "{\"error\":\"ESP32 ยังไม่ได้เชื่อมต่ออินเทอร์เน็ต\"}");
                return;
            }

            String repo = String(configManager.config.githubRepo);
            if (repo.length() == 0) repo = "penpencool/smartwater-master";

            String downloadUrl = server.hasArg("url") ? server.arg("url") : ("https://github.com/" + repo + "/releases/latest/download/firmware.bin");

            Serial.printf("🚀 [GitHub OTA] Starting Cloud download from: %s\n", downloadUrl.c_str());

            // 🛡️ ป้องกันความปลอดภัย: ตัดปั๊มทั้งหมดก่อนอัปเดต
            HardwareController::stopAllOutputs();

            WiFiClientSecure client;
            client.setInsecure();
            client.setTimeout(30);

            httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
            httpUpdate.rebootOnUpdate(false);

            t_httpUpdate_return ret = httpUpdate.update(client, downloadUrl);

            if (ret == HTTP_UPDATE_OK) {
                Serial.println("✅ [GitHub OTA] Update Success!");
                server.send(200, "application/json", "{\"status\":\"OK\", \"msg\":\"✅ อัปเดตเฟิร์มแวร์จาก GitHub สำเร็จ! กำลังรีบูตระบบ...\"}");
                delay(1000);
                ESP.restart();
            } else {
                int errCode = httpUpdate.getLastError();
                String errStr = httpUpdate.getLastErrorString();
                Serial.printf("❌ [GitHub OTA] Failed! Error (%d): %s\n", errCode, errStr.c_str());
                server.send(500, "application/json", "{\"error\":\"อัปเดตล้มเหลว (" + String(errCode) + "): " + errStr + "\"}");
            }
        });
    }
};

#endif // WEB_API_HANDLER_H
