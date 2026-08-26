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
            StaticJsonDocument<2048> doc;

            doc["activeTask"] = activeTaskName;
            doc["errorCode"] = (int)currentError;

            unsigned long nodeOfflineTimeoutMs = (configManager.config.nodeOfflineTimeoutMin > 0) ? ((unsigned long)configManager.config.nodeOfflineTimeoutMin * 60000UL) : 120000UL;

            // Node 1 & Node 2 Status
            bool isNode1Online = (lastNode1Time > 0) && (millis() - lastNode1Time <= nodeOfflineTimeoutMs);
            doc["node1Online"] = isNode1Online;
            doc["node1WaterLow"] = isNode1Online ? pool1Data.waterLow : false;
            doc["node1Battery"] = isNode1Online ? pool1Data.batteryVoltage : 0.0f;
            doc["lastNode1Sec"] = (lastNode1Time > 0) ? ((millis() - lastNode1Time) / 1000) : 9999;
            doc["lastNode1Time"] = lastNode1TimeStr;

            bool isNode2Online = (lastNode2Time > 0) && (millis() - lastNode2Time <= nodeOfflineTimeoutMs);
            doc["node2Online"] = isNode2Online;
            doc["node2WaterLow"] = isNode2Online ? pool2Data.waterLow : false;
            doc["node2Battery"] = isNode2Online ? pool2Data.batteryVoltage : 0.0f;
            doc["lastNode2Sec"] = (lastNode2Time > 0) ? ((millis() - lastNode2Time) / 1000) : 9999;
            doc["lastNode2Time"] = lastNode2TimeStr;

            // Node 3 Online Status
            bool isNode3Online = (lastNode3Time > 0) && (millis() - lastNode3Time <= nodeOfflineTimeoutMs);
            doc["node3Online"] = isNode3Online;
            doc["lastNode3Sec"] = (lastNode3Time > 0) ? ((millis() - lastNode3Time) / 1000) : 9999;
            doc["lastNode3Time"] = lastNode3TimeStr;
            doc["tankLevel"] = isNode3Online ? tankData.waterLevelPercent : 0.0f;
            doc["tankDist"] = isNode3Online ? tankData.distanceCm : -1.0f;
            doc["tankFloat"] = isNode3Online ? tankData.floatBackupActive : false;
            doc["node3Battery"] = isNode3Online ? tankData.batteryVoltage : 0.0f;

            // Node 4 Solar Status
            bool isNode4Online = (lastNode4Time > 0) && (millis() - lastNode4Time <= nodeOfflineTimeoutMs);
            doc["node4Online"] = isNode4Online;
            doc["lastNode4Sec"] = (lastNode4Time > 0) ? ((millis() - lastNode4Time) / 1000) : 9999;
            doc["lastNode4Time"] = lastNode4TimeStr;

            doc["nodeOfflineTimeoutMin"] = configManager.config.nodeOfflineTimeoutMin;

            // Relays Status (All 8 Channels)
            doc["pumpBorehole"] = stateBorehole; // CH1
            doc["pumpFilter"] = stateFilterPump; // CH2/3
            doc["mainA"] = stateMainA;           // CH4
            doc["mainB"] = stateMainB;           // CH5
            doc["sv1"] = stateSV1;               // CH6
            doc["sv2"] = stateSV2;               // CH7
            doc["sv3"] = stateSV3;               // CH7b
            doc["sv4"] = stateSV4;               // CH8
            doc["currentGardenZone"] = currentGardenZone;
            doc["currentPoolTaskZone"] = currentPoolTaskZone;

            // Task Remaining Seconds Countdown
            int taskRemainingSec = 0;
            if (currentGardenZone > 0 && gardenTaskDurationMs > 0) {
                unsigned long elapsed = millis() - gardenTaskStartTime;
                taskRemainingSec = (elapsed < gardenTaskDurationMs) ? ((gardenTaskDurationMs - elapsed) / 1000) : 0;
            } else if (currentPoolTaskZone > 0 && poolTaskDurationMs > 0) {
                unsigned long elapsed = millis() - poolTaskStartTime;
                taskRemainingSec = (elapsed < poolTaskDurationMs) ? ((poolTaskDurationMs - elapsed) / 1000) : 0;
            }
            doc["taskRemainingSec"] = taskRemainingSec;

            // Sensors & Solar
            doc["flowActive"] = (digitalRead(PIN_FLOW_SWITCH) == LOW);
            doc["solarVolt"] = solarData.voltageDC;
            doc["solarWatt"] = solarData.powerWatt;
            doc["solarLux"] = solarData.lightLux;

            // Tank NVS Calibration Params
            doc["tankEmptyCm"] = (tankData.currentEmptyCm > 0) ? tankData.currentEmptyCm : 180.0f;
            doc["tankFullCm"] = (tankData.currentFullCm > 0) ? tankData.currentFullCm : 25.0f;

            // Queue Status & Items
            doc["queueCount"] = taskQueueCount;
            JsonArray queueArr = doc.createNestedArray("queue");
            unsigned long nowMs = millis();
            for (uint8_t i = 0; i < taskQueueCount; i++) {
                JsonObject qObj = queueArr.createNestedObject();
                qObj["type"] = (uint8_t)taskQueue[i].type;
                qObj["name"] = taskQueue[i].taskName;
                qObj["zone"] = taskQueue[i].targetZone;
                qObj["dur"] = taskQueue[i].durationMin;
                
                int delaySec = 0;
                if (taskQueue[i].executeAtMillis > nowMs) {
                    delaySec = (taskQueue[i].executeAtMillis - nowMs) / 1000;
                }
                qObj["delaySec"] = delaySec;
                qObj["isAuto"] = (taskQueue[i].type == TASK_GARDEN_AUTO || taskQueue[i].type == TASK_POOL_AUTO);
            }

            // Pool Auto Top-up Configuration & State
            doc["autoPoolWave"] = configManager.config.autoPoolWaveEnabled;
            doc["poolModeWave"] = configManager.config.poolModeWave;
            doc["poolWaveDelay"] = configManager.config.poolWaveDelayMin;
            doc["poolWaveDur"] = configManager.config.poolWaveDurationMin;

            doc["autoPoolPlay"] = configManager.config.autoPoolPlayEnabled;
            doc["poolModePlay"] = configManager.config.poolModePlay;
            doc["poolPlayDelay"] = configManager.config.poolPlayDelayMin;
            doc["poolPlayDur"] = configManager.config.poolPlayDurationMin;

            // Auto Borehole Refill Thresholds
            doc["autoBorehole"] = configManager.config.autoBoreholeEnabled;
            doc["tankLowTrigger"] = configManager.config.tankLowTrigger;
            doc["tankFullStop"] = configManager.config.tankFullStop;

            // Dynamic Garden Schedules
            doc["schedCount"] = configManager.config.scheduleCount;
            JsonArray schedArr = doc.createNestedArray("schedules");
            for (uint8_t i = 0; i < configManager.config.scheduleCount; i++) {
                JsonObject sObj = schedArr.createNestedObject();
                sObj["enabled"] = configManager.config.schedules[i].enabled;
                sObj["zone"] = configManager.config.schedules[i].zone;
                sObj["days"] = configManager.config.schedules[i].daysOfWeek;
                sObj["startHour"] = configManager.config.schedules[i].startHour;
                sObj["startMin"] = configManager.config.schedules[i].startMin;
                sObj["endHour"] = configManager.config.schedules[i].endHour;
                sObj["endMin"] = configManager.config.schedules[i].endMin;
            }

            doc["currentTime"] = SystemNetwork::getCurrentTimeString();
            doc["ntpSynced"] = ntpSynced;

            // Wi-Fi Status
            bool isWifiConnected = (WiFi.status() == WL_CONNECTED);
            doc["wifiConnected"] = isWifiConnected;
            doc["staIP"] = isWifiConnected ? WiFi.localIP().toString() : "";
            doc["wifiSSID"] = (strlen(configManager.config.wifiSSID) > 0) ? String(configManager.config.wifiSSID) : "";

            int clientCount = WiFi.softAPgetStationNum();
            doc["apClients"] = clientCount;
            doc["apActive"] = SystemNetwork::apActive;
            if (SystemNetwork::apActive) {
                if (clientCount > 0) {
                    doc["apRemainingSec"] = -1;
                } else {
                    unsigned long elapsed = millis() - SystemNetwork::apStartTime;
                    doc["apRemainingSec"] = (elapsed < SystemNetwork::AP_TIMEOUT_MS) ? ((SystemNetwork::AP_TIMEOUT_MS - elapsed) / 1000) : 0;
                }
            } else {
                doc["apRemainingSec"] = 0;
            }

            // Firmware & GitHub Info
            doc["firmwareVer"] = "v1.2.12";
            doc["githubRepo"] = String(configManager.config.githubRepo);

            String response;
            serializeJson(doc, response);
            server.send(200, "application/json", response);
        });

        // 3. Pool Auto Top-Up Config API
        server.on("/api/pool_config", HTTP_POST, [&server, &configManager]() {
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

            configManager.save();
            HardwareController::soundBeep(1, 100);

            Serial.printf("⚙️ Updated Pool Config: Wave[Auto=%d, Mode=%d, Delay=%dm, Dur=%dm], Play[Auto=%d, Mode=%d, Delay=%dm, Dur=%dm]\n",
                          configManager.config.autoPoolWaveEnabled, configManager.config.poolModeWave, configManager.config.poolWaveDelayMin, configManager.config.poolWaveDurationMin,
                          configManager.config.autoPoolPlayEnabled, configManager.config.poolModePlay, configManager.config.poolPlayDelayMin, configManager.config.poolPlayDurationMin);

            server.send(200, "application/json", "{\"msg\":\"✅ บันทึกการตั้งค่าระดับสระว่ายน้ำอัตโนมัติเรียบร้อย\"}");
        });

        // 4. Auto Borehole Config API
        server.on("/api/borehole_config", HTTP_POST, [&server, &configManager]() {
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
                StaticJsonDocument<2048> doc;
                DeserializationError err = deserializeJson(doc, server.arg("plain"));
                if (!err) {
                    JsonArray arr = doc["schedules"].as<JsonArray>();
                    uint8_t count = arr.size();
                    if (count > MAX_SCHEDULE_SLOTS) count = MAX_SCHEDULE_SLOTS;
                    configManager.config.scheduleCount = count;

                    for (uint8_t i = 0; i < count; i++) {
                        configManager.config.schedules[i].enabled = arr[i]["enabled"] | false;
                        configManager.config.schedules[i].zone = arr[i]["zone"] | 1;
                        configManager.config.schedules[i].daysOfWeek = arr[i]["days"] | 127;
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
        server.on("/api/tank_calibrate", HTTP_POST, [&server]() {
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

            TankCalibrationPayload calib;
            calib.msgType = NODE_TANK_CALIBRATE;
            calib.distEmptyCm = emptyVal;
            calib.distFullCm = fullVal;
            calib.commandId = millis();

            uint8_t bcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            esp_err_t res = esp_now_send(bcastMac, (uint8_t *)&calib, sizeof(calib));

            Serial.printf("📡 Sent Calibration to Node 3: Empty=%.1f cm, Full=%.1f cm (Result: %s)\n",
                          emptyVal, fullVal, (res == ESP_OK) ? "OK" : "FAIL");

            HardwareController::soundBeep(1, 150);
            server.send(200, "application/json", "{\"msg\":\"✅ ส่งการตั้งค่าระดับน้ำไปยัง Node 3 เรียบร้อย (Empty: " + String(emptyVal, 0) + "cm, Full: " + String(fullVal, 0) + "cm)\"}");
        });

        // 6. System Parameters Config API (Offline Timeout, etc.)
        server.on("/api/system_config", HTTP_POST, [&server, &configManager]() {
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

            strncpy(configManager.config.wifiSSID, ssid.c_str(), sizeof(configManager.config.wifiSSID) - 1);
            configManager.config.wifiSSID[sizeof(configManager.config.wifiSSID) - 1] = '\0';
            strncpy(configManager.config.wifiPassword, pass.c_str(), sizeof(configManager.config.wifiPassword) - 1);
            configManager.config.wifiPassword[sizeof(configManager.config.wifiPassword) - 1] = '\0';
            configManager.save();

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
            client.setTimeout(15);

            HTTPClient http;
            http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
            http.setTimeout(15000);
            http.begin(client, apiUrl);
            http.setUserAgent("ESP32-SmartWater-Master-OTA");
            http.addHeader("Accept", "application/vnd.github.v3+json");

            int httpCode = http.GET();
            if (httpCode == 200) {
                String payload = http.getString();
                http.end();

                DynamicJsonDocument doc(4096);
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
