#ifndef AUTOMATION_ENGINE_H
#define AUTOMATION_ENGINE_H

#include <Arduino.h>
#include <time.h>

#include "system_pins.h"
#include "system_state.h"
#include "config_manager.h"
#include "hardware_controller.h"
#include "system_network.h"

class AutomationEngine {
public:
    static unsigned long lastBlinkTime;
    static int currentBlinkStep;
    static unsigned long lastHeartbeatTime;
    static bool heartbeatState;

    static void handleStatusIndicators() {
        unsigned long now = millis();

        // 🟢 SYS_LED: Heartbeat (กะพริบช้าเมื่อกำลังรันงาน / ติดค้างเมื่อ Idle)
        if (activeTaskName != "") {
            if (now - lastHeartbeatTime >= 500) {
                lastHeartbeatTime = now;
                heartbeatState = !heartbeatState;
                digitalWrite(PIN_SYS_LED, heartbeatState ? HIGH : LOW);
            }
        } else {
            digitalWrite(PIN_SYS_LED, (currentError == ERR_NONE) ? HIGH : LOW);
        }

        // 🔴 ERR_LED: Blink Count Codes (วนรอบทุก 2 วินาที)
        if (currentError == ERR_NONE) {
            digitalWrite(PIN_ERR_LED, LOW);
            currentBlinkStep = 0;
        } else if (currentError == ERR_ESTOP) {
            digitalWrite(PIN_ERR_LED, HIGH);
        } else {
            uint8_t blinks = (uint8_t)currentError;
            int totalSteps = (blinks * 2) + 10;

            if (now - lastBlinkTime >= 200) {
                lastBlinkTime = now;
                currentBlinkStep = (currentBlinkStep + 1) % totalSteps;

                if (currentBlinkStep < blinks * 2) {
                    digitalWrite(PIN_ERR_LED, (currentBlinkStep % 2 == 0) ? HIGH : LOW);
                } else {
                    digitalWrite(PIN_ERR_LED, LOW);
                }
            }
        }
    }

    static void handleLoop(ConfigManager &configManager) {
        handleStatusIndicators();

        // รักษาเวลาของโหมดจำลองระดับน้ำ Node 3 ไม่ให้ Timeout
        if (simTankActive) {
            StateLock lock;
            lastNode3Time = millis();
            tankData.waterLevelPercent = simTankLevel;
        }

        // รักษาเวลาของโหมดจำลองระดับน้ำสระคลื่น (Node 1)
        if (simPool1Active) {
            StateLock lock;
            lastNode1Time = millis();
            pool1Data.waterLow = simPool1Low;
        }

        // รักษาเวลาของโหมดจำลองระดับน้ำสระเล่น (Node 2)
        if (simPool2Active) {
            StateLock lock;
            lastNode2Time = millis();
            pool2Data.waterLow = simPool2Low;
        }

        // 0. 🛡️ Auto-Recovery / Self-Healing Mechanism (ระบบฟื้นตัวและปลดล็อคตัวเองอัตโนมัติ คนไม่ต้องมากดรีเซ็ต)
        unsigned long nodeOfflineTimeoutMs = (configManager.config.nodeOfflineTimeoutMin > 0) ? ((unsigned long)configManager.config.nodeOfflineTimeoutMin * 60000UL) : 120000UL;
        bool isNode3Offline = (lastNode3Time == 0) || (millis() - lastNode3Time > nodeOfflineTimeoutMs);

        // 🟢 ฟื้นตัวจาก E2 (น้ำในแทงค์ต่ำวิกฤต): เมื่อน้ำถูกเติมขึ้นมาเกินระดับปลอดภัย + Hysteresis 3% -> ปลดล็อค Alarm เองอัตโนมัติ
        if (currentError == ERR_TANK_DRY) {
            float recoveryThreshold = configManager.config.tankSafeCutoff + 3.0f; // เช่น 25% + 3% = 28%
            if (!isNode3Offline && tankData.waterLevelPercent > recoveryThreshold) {
                currentError = ERR_NONE;
                Serial.printf("🟢 [AUTO RECOVERY] Tank water level restored (%.1f%% > %.1f%%) -> Cleared Alarm E2 automatically.\n",
                              tankData.waterLevelPercent, recoveryThreshold);
            }
        }

        // 🟢 ฟื้นตัวจาก E4 (เซ็นเซอร์หลุด): เมื่อโหนดกลับมาส่งข้อมูลได้ตามปกติ -> ปลดล็อค Alarm เองอัตโนมัติ
        if (currentError == ERR_NODE_LOST) {
            if (!isNode3Offline) {
                currentError = ERR_NONE;
                Serial.println("🟢 [AUTO RECOVERY] Node 3 Tank Sensor reconnected -> Cleared Alarm E4 automatically.");
            }
        }

        // 0b. 🌙 Power Management & Deep Sleep Schedule (Master Controller)
        if (configManager.config.masterSleepEnabled && ntpSynced) {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 0)) {
                int nowM = timeinfo.tm_hour * 60 + timeinfo.tm_min;
                int startM = configManager.config.activeStartHour * 60 + configManager.config.activeStartMin;
                int endM = configManager.config.activeEndHour * 60 + configManager.config.activeEndMin;

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

                // หากอยู่นอกเวลาทำงาน และไม่มีงานสูบน้ำค้างอยู่ ให้เข้าโหมด Deep Sleep ทันที
                if (isOffHours && minsUntilWake > 0 && currentGardenZone == 0 && currentPoolTaskZone == 0 && !stateBorehole) {
                    Serial.printf("🌙 [POWER MGMT] Reached end of active window (%02d:%02d - %02d:%02d). Shutting down all loads and entering Deep Sleep for %d minutes (until %02d:%02d)...\n",
                                  configManager.config.activeStartHour, configManager.config.activeStartMin,
                                  configManager.config.activeEndHour, configManager.config.activeEndMin,
                                  minsUntilWake,
                                  configManager.config.activeStartHour, configManager.config.activeStartMin);

                    HardwareController::stopAllOutputs();
                    digitalWrite(PIN_SYS_LED, LOW);
                    digitalWrite(PIN_ERR_LED, LOW);
                    HardwareController::soundBeep(1, 400);
                    delay(200);

                    uint64_t sleepUs = (uint64_t)minsUntilWake * 60ULL * 1000000ULL;
                    esp_sleep_enable_timer_wakeup(sleepUs);
                    esp_deep_sleep_start();
                }
            }
        }

        // 1. Pool Top-Up Auto Task Timer
        if (currentPoolTaskZone > 0) {
            if (millis() - poolTaskStartTime >= poolTaskDurationMs) {
                // เช็คโหมด: ถ้าเป็นโหมด 1 (เติมจนกว่าจะถึงระดับเต็ม) และลูกลอยยังไม่เต็ม ให้เติมรอบต่อไปทันที
                if (currentPoolTaskZone == 1 && configManager.config.poolModeWave == 1 && pool1Data.waterLow) {
                    Serial.println("🌊 [AUTO Mode: เติมจนเต็ม] หมดรอบเวลาแล้วแต่น้ำยังไม่เต็ม -> ต่อเวลาเติมสระคลื่นอีก 15 นาที...");
                    poolTaskStartTime = millis();
                    poolTaskDurationMs = (unsigned long)configManager.config.poolWaveDurationMin * 60000UL;
                } else if (currentPoolTaskZone == 2 && configManager.config.poolModePlay == 1 && pool2Data.waterLow) {
                    Serial.println("🏊 [AUTO Mode: เติมจนเต็ม] หมดรอบเวลาแล้วแต่น้ำยังไม่เต็ม -> ต่อเวลาเติมสระเล่นอีก 15 นาที...");
                    poolTaskStartTime = millis();
                    poolTaskDurationMs = (unsigned long)configManager.config.poolPlayDurationMin * 60000UL;
                } else {
                    Serial.printf("🌊 Pool Top-up Zone %d finished. Stopping...\n", currentPoolTaskZone);
                    HardwareController::stopPoolTopUp();
                    HardwareController::soundBeep(2, 100);
                }
            }
        }

        // 2. Garden Sprinkler Auto Task & Schedule
        if (currentGardenZone > 0) {
            if (millis() - gardenTaskStartTime >= gardenTaskDurationMs) {
                Serial.printf("🌱 Garden Sprinkler Zone %d finished. Stopping...\n", currentGardenZone);
                HardwareController::stopGardenSprinkler();
                HardwareController::soundBeep(2, 100);
            }
        } else if (currentPoolTaskZone == 0) {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo, 0)) {
                for (uint8_t i = 0; i < configManager.config.scheduleCount; i++) {
                    ScheduleSlot &slot = configManager.config.schedules[i];
                    if (slot.enabled && timeinfo.tm_hour == slot.startHour && timeinfo.tm_min == slot.startMin) {
                        if (lastRunMinuteZ1 != (slot.startHour * 100 + slot.startMin + i)) {
                            lastRunMinuteZ1 = (slot.startHour * 100 + slot.startMin + i);

                            int startTotal = slot.startHour * 60 + slot.startMin;
                            int endTotal = slot.endHour * 60 + slot.endMin;
                            int durationMin = endTotal - startTotal;
                            if (durationMin <= 0) durationMin += 24 * 60;

                            if (!stateBorehole && currentError == ERR_NONE) {
                                float startThreshold = (slot.zone == 1) ? configManager.config.gardenZ1StartLevel : configManager.config.gardenZ2StartLevel;
                                if (tankData.waterLevelPercent >= startThreshold || tankData.waterLevelPercent <= 0.0f) {
                                    Serial.printf("⏰ [SCHEDULE Slot %d] Triggering Zone %d (%02d:%02d - %02d:%02d, %d min)!\n",
                                                  i + 1, slot.zone, slot.startHour, slot.startMin, slot.endHour, slot.endMin, durationMin);
                                    HardwareController::startGardenZone(slot.zone, durationMin);
                                } else {
                                    Serial.printf("⏰ [SCHEDULE Slot %d] Tank level (%.1f%%) < Start threshold (%.1f%%) -> Queued Zone %d for auto-start when filled.\n",
                                                  i + 1, tankData.waterLevelPercent, startThreshold, slot.zone);
                                    TaskQueueManager::enqueue(TASK_GARDEN_AUTO, slot.zone, durationMin, "รดน้ำโซน " + String(slot.zone) + " (ตามตาราง - รอน้ำถึง " + String((int)startThreshold) + "%)");
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        // 3. Flow Switch Watchdog & Live Tank Safety Check with Smart Zone Cutoff / Auto-Pause
        if (stateFilterPump) {
            // หากกำลังปั๊มน้ำออกอยู่แล้วพบว่า Node 3 หลุด -> ตัดปั๊มทันทีเพื่อความปลอดภัย
            if (isNode3Offline) {
                HardwareController::stopAllOutputs();
                currentError = ERR_NODE_LOST;
                HardwareController::soundBeep(3, 200);
                Serial.println("❌ ERROR: Node 3 Tank Sensor lost during pumping! Emergency shutoff.");
            }
            // 💧 ตรวจสอบเกณฑ์หยุดพักของการรดน้ำ Zone 1
            else if (currentGardenZone == 1 && tankData.waterLevelPercent <= configManager.config.gardenZ1StopLevel && tankData.waterLevelPercent > 0.0f) {
                unsigned long elapsed = millis() - gardenTaskStartTime;
                uint16_t remMin = (elapsed < gardenTaskDurationMs) ? ((gardenTaskDurationMs - elapsed) / 60000UL) : 0;
                if (remMin == 0) remMin = 1;
                TaskQueueManager::enqueue(TASK_GARDEN_AUTO, 1, remMin, "รดน้ำโซน 1 (พักรอน้ำในแทงค์ถึง " + String((int)configManager.config.gardenZ1StartLevel) + "%)");
                Serial.printf("⏸️ [AUTO PAUSE] Tank level (%.1f%%) <= Stop threshold (%.1f%%) -> Paused Zone 1, rem %d min. Waiting for borehole refill to %.1f%%.\n",
                              tankData.waterLevelPercent, configManager.config.gardenZ1StopLevel, remMin, configManager.config.gardenZ1StartLevel);
                HardwareController::stopGardenSprinkler();
                HardwareController::soundBeep(2, 150);
            }
            // 💧 ตรวจสอบเกณฑ์หยุดพักของการรดน้ำ Zone 2
            else if (currentGardenZone == 2 && tankData.waterLevelPercent <= configManager.config.gardenZ2StopLevel && tankData.waterLevelPercent > 0.0f) {
                unsigned long elapsed = millis() - gardenTaskStartTime;
                uint16_t remMin = (elapsed < gardenTaskDurationMs) ? ((gardenTaskDurationMs - elapsed) / 60000UL) : 0;
                if (remMin == 0) remMin = 1;
                TaskQueueManager::enqueue(TASK_GARDEN_AUTO, 2, remMin, "รดน้ำโซน 2 (พักรอน้ำในแทงค์ถึง " + String((int)configManager.config.gardenZ2StartLevel) + "%)");
                Serial.printf("⏸️ [AUTO PAUSE] Tank level (%.1f%%) <= Stop threshold (%.1f%%) -> Paused Zone 2, rem %d min. Waiting for borehole refill to %.1f%%.\n",
                              tankData.waterLevelPercent, configManager.config.gardenZ2StopLevel, remMin, configManager.config.gardenZ2StartLevel);
                HardwareController::stopGardenSprinkler();
                HardwareController::soundBeep(2, 150);
            }
            // 💧 ตรวจสอบเกณฑ์หยุดพักของการเติมน้ำสระคลื่น (Wave Pool - SV1)
            else if (currentPoolTaskZone == 1 && tankData.waterLevelPercent <= configManager.config.poolWaveStopLevel && tankData.waterLevelPercent > 0.0f) {
                unsigned long elapsed = millis() - poolTaskStartTime;
                uint16_t remMin = (elapsed < poolTaskDurationMs) ? ((poolTaskDurationMs - elapsed) / 60000UL) : 0;
                if (remMin == 0) remMin = 1;
                TaskType pType = isManualPoolTask ? TASK_POOL_MANUAL : TASK_POOL_AUTO;
                TaskQueueManager::enqueue(pType, 1, remMin, "เติมสระคลื่น (พักรอน้ำในแทงค์ถึง " + String((int)configManager.config.poolWaveStartLevel) + "%)");
                Serial.printf("⏸️ [AUTO PAUSE] Tank level (%.1f%%) <= Stop threshold (%.1f%%) -> Paused Wave Pool, rem %d min. Waiting for borehole refill to %.1f%%.\n",
                              tankData.waterLevelPercent, configManager.config.poolWaveStopLevel, remMin, configManager.config.poolWaveStartLevel);
                HardwareController::stopPoolTopUp();
                HardwareController::soundBeep(2, 150);
            }
            // 💧 ตรวจสอบเกณฑ์หยุดพักของการเติมน้ำสระเล่น (Play Pool - SV2)
            else if (currentPoolTaskZone == 2 && tankData.waterLevelPercent <= configManager.config.poolPlayStopLevel && tankData.waterLevelPercent > 0.0f) {
                unsigned long elapsed = millis() - poolTaskStartTime;
                uint16_t remMin = (elapsed < poolTaskDurationMs) ? ((poolTaskDurationMs - elapsed) / 60000UL) : 0;
                if (remMin == 0) remMin = 1;
                TaskType pType = isManualPoolTask ? TASK_POOL_MANUAL : TASK_POOL_AUTO;
                TaskQueueManager::enqueue(pType, 2, remMin, "เติมสระเล่น (พักรอน้ำในแทงค์ถึง " + String((int)configManager.config.poolPlayStartLevel) + "%)");
                Serial.printf("⏸️ [AUTO PAUSE] Tank level (%.1f%%) <= Stop threshold (%.1f%%) -> Paused Play Pool, rem %d min. Waiting for borehole refill to %.1f%%.\n",
                              tankData.waterLevelPercent, configManager.config.poolPlayStopLevel, remMin, configManager.config.poolPlayStartLevel);
                HardwareController::stopPoolTopUp();
                HardwareController::soundBeep(2, 150);
            }
            // 🛡️ Hard Dry-Run Cutoff ทั่วไป (เช่น Tank Safe Cutoff)
            else if (tankData.waterLevelPercent <= configManager.config.tankSafeCutoff && tankData.waterLevelPercent > 0.0f) {
                HardwareController::stopAllOutputs();
                currentError = ERR_TANK_DRY;
                HardwareController::soundBeep(3, 200);
                Serial.printf("❌ ERROR: Tank Water Level critically low (%.1f%% <= %.1f%%)! Emergency shutoff.\n",
                              tankData.waterLevelPercent, configManager.config.tankSafeCutoff);
            }
        }

        if (flowWatchdogActive) {
            bool waterFlowing = (digitalRead(PIN_FLOW_SWITCH) == LOW);
            unsigned long elapsed = (millis() - filterPumpStartTime) / 1000;

            if (elapsed >= configManager.config.flowTimeoutSec) {
                if (!waterFlowing) {
                    HardwareController::stopAllOutputs();
                    currentError = ERR_NO_FLOW;
                    HardwareController::soundBeep(3, 200);
                    Serial.println("❌ ERROR: No Flow Detected! Emergency Shutoff.");
                } else {
                    flowWatchdogActive = false;
                }
            }
        }

        // 4. Auto Borehole Tank Refill Logic (คำนวณจากระดับน้ำแทงค์ %)
        // 💧 ปั๊มบาดาลสูบน้ำเข้าแทงค์: ต้องทำงานได้เสมอแม้ติด ERR_TANK_DRY (เพราะน้ำแห้งจึงต้องสูบน้ำเข้า!)
        bool boreholeAllowed = (currentError == ERR_NONE || currentError == ERR_TANK_DRY);
        if (configManager.config.autoBoreholeEnabled && !isNode3Offline && boreholeAllowed) {
            // หากระดับน้ำต่ำกว่า Trigger (< 70%) และปั๊มดันน้ำไม่ได้ทำงานอยู่
            if (!stateBorehole && tankData.waterLevelPercent > 0.0f && tankData.waterLevelPercent < configManager.config.tankLowTrigger) {
                if (!stateFilterPump && !tankData.floatBackupActive) {
                    stateBorehole = true;
                    HardwareController::setRelay(RELAY_BOREHOLE, true);
                    HardwareController::soundBeep(1, 150);
                    Serial.printf("⚡ [AUTO BOREHOLE] Tank level (%.1f%%) < Low Trigger (%.1f%%) -> Started Borehole Pump.\n",
                                  tankData.waterLevelPercent, configManager.config.tankLowTrigger);
                }
            }
            // หากระดับน้ำเต็มตามเป้า (>= 95%) หรือลูกลอยตัด -> สั่งปิดปั๊มบาดาล
            else if (stateBorehole && (tankData.waterLevelPercent >= configManager.config.tankFullStop || tankData.floatBackupActive)) {
                stateBorehole = false;
                HardwareController::setRelay(RELAY_BOREHOLE, false);
                HardwareController::soundBeep(2, 100);
                Serial.printf("⚡ [AUTO BOREHOLE] Tank full (%.1f%%) or float backup triggered -> Stopped Borehole Pump.\n",
                              tankData.waterLevelPercent);
            }
        }

        // 5. Pool Auto Top-up Queueing (State-Aware: เติมน้ำอัตโนมัติตลอดเวลาที่ลูกลอยตก แม้เพิ่งผ่าน E-Stop หรือ Reset)
        bool isNode1Online = (lastNode1Time > 0) && (millis() - lastNode1Time <= nodeOfflineTimeoutMs);
        bool isNode2Online = (lastNode2Time > 0) && (millis() - lastNode2Time <= nodeOfflineTimeoutMs);

        // ตรวจจับลูกลอยลดระดับ (Node 1: สระคลื่น)
        if (configManager.config.autoPoolWaveEnabled && isNode1Online && pool1Data.waterLow) {
            bool inQueue = TaskQueueManager::isQueued(TASK_POOL_AUTO, 1) || TaskQueueManager::isQueued(TASK_POOL_MANUAL, 1);
            if (!inQueue && currentPoolTaskZone != 1) {
                if (poolLowStartTimeWave == 0) {
                    poolLowStartTimeWave = millis();
                }
                unsigned long elapsed = millis() - poolLowStartTimeWave;
                unsigned long delayMs = (unsigned long)configManager.config.poolWaveDelayMin * 60000UL;
                unsigned long remDelay = (elapsed < delayMs) ? (delayMs - elapsed) : 0;
                TaskQueueManager::enqueue(TASK_POOL_AUTO, 1, configManager.config.poolWaveDurationMin, "เติมน้ำสระคลื่นอัตโนมัติ", remDelay);
                Serial.printf("🌊 [AUTO] Wave Pool Low -> Queued auto top-up (Delay remaining: %lu ms).\n", remDelay);
            }
        } else if (!pool1Data.waterLow) {
            poolLowStartTimeWave = 0;
            TaskQueueManager::remove(TASK_POOL_AUTO, 1);
            TaskQueueManager::remove(TASK_POOL_MANUAL, 1);
            // 🛡️ ป้องกันน้ำล้น: หากสระคลื่นน้ำเต็มแล้ว สั่งตัดหยุดทันทีทั้งโหมดอัตโนมัติและแมนนวล
            if (currentPoolTaskZone == 1) {
                Serial.println("🌊 [SAFETY] Wave Pool float restored to FULL -> Stopping top-up immediately.");
                isManualPoolTask = false;
                HardwareController::stopPoolTopUp();
            }
        }

        // ตรวจจับลูกลอยลดระดับ (Node 2: สระเล่น)
        if (configManager.config.autoPoolPlayEnabled && isNode2Online && pool2Data.waterLow) {
            bool inQueue = TaskQueueManager::isQueued(TASK_POOL_AUTO, 2) || TaskQueueManager::isQueued(TASK_POOL_MANUAL, 2);
            if (!inQueue && currentPoolTaskZone != 2) {
                if (poolLowStartTimePlay == 0) {
                    poolLowStartTimePlay = millis();
                }
                unsigned long elapsed = millis() - poolLowStartTimePlay;
                unsigned long delayMs = (unsigned long)configManager.config.poolPlayDelayMin * 60000UL;
                unsigned long remDelay = (elapsed < delayMs) ? (delayMs - elapsed) : 0;
                TaskQueueManager::enqueue(TASK_POOL_AUTO, 2, configManager.config.poolPlayDurationMin, "เติมน้ำสระเล่นอัตโนมัติ", remDelay);
                Serial.printf("🏊 [AUTO] Play Pool Low -> Queued auto top-up (Delay remaining: %lu ms).\n", remDelay);
            }
        } else if (!pool2Data.waterLow) {
            poolLowStartTimePlay = 0;
            TaskQueueManager::remove(TASK_POOL_AUTO, 2);
            TaskQueueManager::remove(TASK_POOL_MANUAL, 2);
            // 🛡️ ป้องกันน้ำล้น: หากสระเล่นน้ำเต็มแล้ว สั่งตัดหยุดทันทีทั้งโหมดอัตโนมัติและแมนนวล
            if (currentPoolTaskZone == 2) {
                Serial.println("🏊 [SAFETY] Play Pool float restored to FULL -> Stopping top-up immediately.");
                isManualPoolTask = false;
                HardwareController::stopPoolTopUp();
            }
        }

        // 6. 🚀 Priority Task Queue Dispatcher (ประมวลผลคิวตามลำดับความสำคัญเมื่อระบบว่าง และระดับน้ำถึงเกณฑ์เริ่ม)
        if (currentGardenZone == 0 && currentPoolTaskZone == 0 && !stateBorehole && currentError == ERR_NONE) {
            TaskQueueItem nextTask;
            for (uint8_t i = 0; i < taskQueueCount; i++) {
                if (millis() >= taskQueue[i].executeAtMillis) {
                    float reqStart = 0.0f;
                    if (taskQueue[i].type == TASK_GARDEN_MANUAL || taskQueue[i].type == TASK_GARDEN_AUTO) {
                        reqStart = (taskQueue[i].targetZone == 1) ? configManager.config.gardenZ1StartLevel : configManager.config.gardenZ2StartLevel;
                    } else if (taskQueue[i].type == TASK_POOL_MANUAL || taskQueue[i].type == TASK_POOL_AUTO) {
                        reqStart = (taskQueue[i].targetZone == 1) ? configManager.config.poolWaveStartLevel : configManager.config.poolPlayStartLevel;
                    }

                    // ถ้าน้ำในแทงค์ถึงเกณฑ์ที่กำหนด ให้ดึงงานไปทำงานทันที
                    if (tankData.waterLevelPercent >= reqStart || tankData.waterLevelPercent <= 0.0f) {
                        if (TaskQueueManager::popAtIndex(i, nextTask)) {
                            Serial.printf("🎯 [QUEUE DISPATCH] Executing task: %s (Type %d, Zone %d, %d min) [Tank %.1f%% >= Req %.1f%%]\n",
                                          nextTask.taskName.c_str(), nextTask.type, nextTask.targetZone, nextTask.durationMin,
                                          tankData.waterLevelPercent, reqStart);

                            if (nextTask.type == TASK_GARDEN_MANUAL || nextTask.type == TASK_GARDEN_AUTO) {
                                HardwareController::startGardenZone(nextTask.targetZone, nextTask.durationMin);
                            } else if (nextTask.type == TASK_POOL_MANUAL || nextTask.type == TASK_POOL_AUTO) {
                                isManualPoolTask = (nextTask.type == TASK_POOL_MANUAL);
                                HardwareController::startPoolTopUp(nextTask.targetZone, nextTask.durationMin);
                            }
                            break;
                        }
                    }
                }
            }
        }

        // Node 3 Offline Watchdog Safety
        if (isNode3Offline && stateBorehole) {
            stateBorehole = false;
            HardwareController::setRelay(RELAY_BOREHOLE, false);
            currentError = ERR_NODE_LOST;
            HardwareController::soundBeep(3, 100);
            Serial.println("⚠️ [SAFETY] Node 3 Offline while Borehole Pump running! Cutoff pump for safety.");
        }
    }
};

#endif // AUTOMATION_ENGINE_H
