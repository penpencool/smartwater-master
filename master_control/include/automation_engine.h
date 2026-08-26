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
        StateLock lock;
        handleStatusIndicators();

        // รักษาเวลาของโหมดจำลองระดับน้ำ Node 3 ไม่ให้ Timeout
        if (simTankActive) {
            lastNode3Time = millis();
            tankData.waterLevelPercent = simTankLevel;
        }

        // รักษาเวลาของโหมดจำลองระดับน้ำสระคลื่น (Node 1)
        if (simPool1Active) {
            lastNode1Time = millis();
            pool1Data.waterLow = simPool1Low;
        }

        // รักษาเวลาของโหมดจำลองระดับน้ำสระเล่น (Node 2)
        if (simPool2Active) {
            lastNode2Time = millis();
            pool2Data.waterLow = simPool2Low;
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
            if (getLocalTime(&timeinfo, 10)) {
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
                                Serial.printf("⏰ [SCHEDULE Slot %d] Triggering Zone %d (%02d:%02d - %02d:%02d, %d min)!\n",
                                              i + 1, slot.zone, slot.startHour, slot.startMin, slot.endHour, slot.endMin, durationMin);
                                HardwareController::startGardenZone(slot.zone, durationMin);
                                break;
                            }
                        }
                    }
                }
            }
        }

        // 3. Flow Switch Watchdog & Live Tank Safety Check
        if (stateFilterPump) {
            unsigned long nodeOfflineTimeoutMs = (configManager.config.nodeOfflineTimeoutMin > 0) ? ((unsigned long)configManager.config.nodeOfflineTimeoutMin * 60000UL) : 120000UL;
            bool isNode3Offline = (lastNode3Time == 0) || (millis() - lastNode3Time > nodeOfflineTimeoutMs);

            // หากกำลังปั๊มน้ำอยู่แล้วพบว่า Node 3 หลุด หรือน้ำในแทงค์ต่ำวิกฤต -> ตัดปั๊มทันที
            if (isNode3Offline) {
                HardwareController::stopAllOutputs();
                currentError = ERR_NODE_LOST;
                HardwareController::soundBeep(3, 200);
                Serial.println("❌ ERROR: Node 3 Tank Sensor lost during pumping! Emergency shutoff.");
            } else if (tankData.waterLevelPercent <= configManager.config.tankSafeCutoff && tankData.waterLevelPercent > 0.0f) {
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
        unsigned long nodeOfflineTimeoutMs = (configManager.config.nodeOfflineTimeoutMin > 0) ? ((unsigned long)configManager.config.nodeOfflineTimeoutMin * 60000UL) : 120000UL;
        bool isNode3Offline = (lastNode3Time == 0) || (millis() - lastNode3Time > nodeOfflineTimeoutMs);
        if (configManager.config.autoBoreholeEnabled && !isNode3Offline && currentError == ERR_NONE) {
            // หากระดับน้ำต่ำกว่า Trigger (< 70%) และปั๊มดันน้ำไม่ได้ทำงานอยู่
            if (!stateBorehole && tankData.waterLevelPercent > 0.0f && tankData.waterLevelPercent < configManager.config.tankLowTrigger) {
                if (!stateFilterPump && !tankData.floatBackupActive) {
                    stateBorehole = true;
                    HardwareController::setRelay(RELAY_BOREHOLE, true);
                    HardwareController::soundBeep(1, 150);
                    Serial.printf("⚡ [AUTO] Tank level (%.1f%%) < Low Trigger (%.1f%%) -> Started Borehole Pump.\n",
                                  tankData.waterLevelPercent, configManager.config.tankLowTrigger);
                }
            }
            // หากระดับน้ำเต็มตามเป้า (>= 95%) หรือลูกลอยตัด -> สั่งปิดปั๊มบาดาล
            else if (stateBorehole && (tankData.waterLevelPercent >= configManager.config.tankFullStop || tankData.floatBackupActive)) {
                stateBorehole = false;
                HardwareController::setRelay(RELAY_BOREHOLE, false);
                HardwareController::soundBeep(2, 100);
                Serial.printf("⚡ [AUTO] Tank full (%.1f%%) or float backup triggered -> Stopped Borehole Pump.\n",
                              tankData.waterLevelPercent);
            }
        }

        // 5. Pool Auto Top-up Queueing
        bool isNode1Online = (lastNode1Time > 0) && (millis() - lastNode1Time <= nodeOfflineTimeoutMs);
        bool isNode2Online = (lastNode2Time > 0) && (millis() - lastNode2Time <= nodeOfflineTimeoutMs);

        // ตรวจจับลูกลอยลดระดับ (Node 1: สระคลื่น)
        if (configManager.config.autoPoolWaveEnabled && isNode1Online && pool1Data.waterLow) {
            if (poolLowStartTimeWave == 0) {
                poolLowStartTimeWave = millis();
                unsigned long delayMs = (unsigned long)configManager.config.poolWaveDelayMin * 60000UL;
                TaskQueueManager::enqueue(TASK_POOL_AUTO, 1, configManager.config.poolWaveDurationMin, "เติมน้ำสระคลื่นอัตโนมัติ", delayMs);
                Serial.printf("🌊 [AUTO] Wave Pool Low -> Queued auto top-up with %d min delay.\n", configManager.config.poolWaveDelayMin);
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
            if (poolLowStartTimePlay == 0) {
                poolLowStartTimePlay = millis();
                unsigned long delayMs = (unsigned long)configManager.config.poolPlayDelayMin * 60000UL;
                TaskQueueManager::enqueue(TASK_POOL_AUTO, 2, configManager.config.poolPlayDurationMin, "เติมน้ำสระเล่นอัตโนมัติ", delayMs);
                Serial.printf("🏊 [AUTO] Play Pool Low -> Queued auto top-up with %d min delay.\n", configManager.config.poolPlayDelayMin);
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

        // 6. 🚀 Priority Task Queue Dispatcher (ประมวลผลคิวตามลำดับความสำคัญเมื่อระบบว่าง และถึงเวลา execute)
        if (currentGardenZone == 0 && currentPoolTaskZone == 0 && !stateBorehole && currentError == ERR_NONE) {
            TaskQueueItem nextTask;
            if (TaskQueueManager::popReadyHighest(nextTask)) {
                Serial.printf("🎯 [QUEUE DISPATCH] Executing next highest priority task: %s (Type %d, Zone %d, %d min)\n",
                              nextTask.taskName.c_str(), nextTask.type, nextTask.targetZone, nextTask.durationMin);

                if (nextTask.type == TASK_GARDEN_MANUAL || nextTask.type == TASK_GARDEN_AUTO) {
                    HardwareController::startGardenZone(nextTask.targetZone, nextTask.durationMin);
                } else if (nextTask.type == TASK_POOL_MANUAL || nextTask.type == TASK_POOL_AUTO) {
                    isManualPoolTask = (nextTask.type == TASK_POOL_MANUAL);
                    HardwareController::startPoolTopUp(nextTask.targetZone, nextTask.durationMin);
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
