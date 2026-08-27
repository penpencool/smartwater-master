#ifndef HARDWARE_CONTROLLER_H
#define HARDWARE_CONTROLLER_H

#include <Arduino.h>
#include "system_pins.h"
#include "system_state.h"
#include "config_manager.h"

class HardwareController {
public:
    static void init() {
        pinMode(RELAY_BOREHOLE, OUTPUT);
        pinMode(RELAY_FILTER_ON, OUTPUT);
        pinMode(RELAY_FILTER_OFF, OUTPUT);
        pinMode(RELAY_MAIN_A, OUTPUT);
        pinMode(RELAY_MAIN_B, OUTPUT);
        pinMode(RELAY_SV1, OUTPUT);
        pinMode(RELAY_SV2, OUTPUT);
        pinMode(RELAY_SV3, OUTPUT);
        pinMode(RELAY_SV4, OUTPUT);

        pinMode(PIN_FLOW_SWITCH, INPUT_PULLUP);
        pinMode(PIN_SYS_LED, OUTPUT);
        pinMode(PIN_ERR_LED, OUTPUT);
        pinMode(PIN_BUZZER, OUTPUT);

        stopAllOutputs();
    }

    static void setRelay(uint8_t pin, bool active) {
        digitalWrite(pin, active ? HIGH : LOW);
    }

    static void triggerPulse(uint8_t pin, uint16_t durationMs = 400) {
        digitalWrite(pin, HIGH);
        delay(durationMs);
        digitalWrite(pin, LOW);
    }

    static void soundBeep(uint8_t count = 1, uint16_t durationMs = 100) {
        for (uint8_t i = 0; i < count; i++) {
            digitalWrite(PIN_BUZZER, HIGH);
            delay(durationMs);
            digitalWrite(PIN_BUZZER, LOW);
            if (i < count - 1) delay(100);
        }
    }

    static void stopAllOutputs() {
        {
            StateLock lock;
            stateBorehole = false;
            stateFilterPump = false;
            stateMainA = false;
            stateMainB = false;
            stateSV1 = false;
            stateSV2 = false;
            stateSV3 = false;
            stateSV4 = false;

            flowWatchdogActive = false;
            isManualPoolTask = false; // BUG-1 FIX: รีเซ็ต flag เพื่อไม่ให้ค้างหลัง Safety Cutoff
            currentGardenZone = 0;
            currentPoolTaskZone = 0;
            activeTaskName = "";
            TaskQueueManager::clear();
        }

        setRelay(RELAY_BOREHOLE, false);
        triggerPulse(RELAY_FILTER_OFF, 400);
        setRelay(RELAY_MAIN_A, false);
        setRelay(RELAY_MAIN_B, false);
        setRelay(RELAY_SV1, false);
        setRelay(RELAY_SV2, false);
        setRelay(RELAY_SV3, false);
        setRelay(RELAY_SV4, false);
    }

    static void startFilterPump() {
        {
            StateLock lock;
            stateFilterPump = true;
            filterPumpStartTime = millis();
            flowWatchdogActive = true;
        }
        triggerPulse(RELAY_FILTER_ON, 400);
        soundBeep(1, 150);
    }

    static void stopFilterPump() {
        {
            StateLock lock;
            stateFilterPump = false;
            flowWatchdogActive = false;
        }
        triggerPulse(RELAY_FILTER_OFF, 400);
        soundBeep(1, 100);
    }

    static bool startGardenZone(uint8_t zone, uint16_t durationMin) {
        StateLock lock;
        if (currentError != ERR_NONE) {
            Serial.printf("❌ Cannot start garden sprinkler: System in Error State (E%d)\n", currentError);
            return false;
        }

        if (stateBorehole) {
            Serial.println("⚠️ Cannot start garden sprinkler: Borehole pump running (Interlock)");
            return false;
        }

        // 🛡️ ความปลอดภัยแทงค์น้ำ: หาก Node 3 ขาดการติดต่อ ไม่รู้ระดับน้ำ หรือน้ำในแทงค์ต่ำวิกฤต
        // BUG-3 FIX: ใช้ค่า nodeOfflineTimeoutMin จาก config แทน hardcoded 120s
        unsigned long nodeOfflineTimeoutMs = (configManager.config.nodeOfflineTimeoutMin > 0) ? ((unsigned long)configManager.config.nodeOfflineTimeoutMin * 60000UL) : 120000UL;
        if (lastNode3Time == 0 || millis() - lastNode3Time > nodeOfflineTimeoutMs) {
            Serial.println("❌ Cannot start garden sprinkler: Node 3 Tank Sensor Offline! Aborting for safety.");
            currentError = ERR_NODE_LOST;
            soundBeep(3, 200);
            return false;
        }
        if (tankData.waterLevelPercent <= configManager.config.tankSafeCutoff && tankData.waterLevelPercent > 0.0f) {
            Serial.println("❌ Cannot start garden sprinkler: Tank Water Level Critically Low! Aborting.");
            currentError = ERR_TANK_DRY;
            soundBeep(3, 200);
            return false;
        }

        // หากกำลังเติมน้ำสระอยู่ (Pool Top-Up ซึ่งเป็น Priority ต่ำกว่า) ให้พักงานเติมสระและโยนกลับเข้าคิว
        if (currentPoolTaskZone > 0) {
            unsigned long elapsed = millis() - poolTaskStartTime;
            uint16_t remMin = (elapsed < poolTaskDurationMs) ? ((poolTaskDurationMs - elapsed) / 60000UL) : 0;
            if (remMin == 0) remMin = 1;

            // BUG-4 FIX: ใช้ isManualPoolTask เป็นตัวตัดสินประเภทงานแทนการ assume จาก zone
            TaskType pType = isManualPoolTask ? TASK_POOL_MANUAL : TASK_POOL_AUTO;
            String pName = (currentPoolTaskZone == 1) ? "เติมสระคลื่น (พักไว้)" : "เติมสระเล่น (พักไว้)";
            TaskQueueManager::enqueue(pType, currentPoolTaskZone, remMin, pName);

            Serial.printf("⏸️ Pausing Pool Top-up (Zone %d, Rem %d min) -> Queued for resume.\n", currentPoolTaskZone, remMin);
            stopPoolTopUp();
        }

        // Step 1: ปิด Main A และโซนอื่นๆ ก่อน
        stateMainA = false;
        setRelay(RELAY_MAIN_A, false);
        stateSV1 = false;
        setRelay(RELAY_SV1, false);
        stateSV2 = false;
        setRelay(RELAY_SV2, false);

        if (zone == 1) {
            stateSV4 = false;
            setRelay(RELAY_SV4, false);
            stateSV3 = true;
            setRelay(RELAY_SV3, true);
        } else if (zone == 2) {
            stateSV3 = false;
            setRelay(RELAY_SV3, false);
            stateSV4 = true;
            setRelay(RELAY_SV4, true);
        } else {
            return false;
        }

        // Step 2: เปิด Main Solenoid B (Bypass รดน้ำ)
        stateMainB = true;
        setRelay(RELAY_MAIN_B, true);
        delay(500);

        // Step 3: สตาร์ทปั๊มดันน้ำ
        startFilterPump();

        currentGardenZone = zone;
        gardenTaskStartTime = millis();
        gardenTaskDurationMs = (unsigned long)durationMin * 60000UL;
        activeTaskName = "รดน้ำโซน " + String(zone) + " (" + String(durationMin) + " นาที)";

        Serial.printf("🌱 Started Garden Sprinkler Zone %d for %d minutes.\n", zone, durationMin);
        return true;
    }

    static void stopGardenSprinkler() {
        StateLock lock;
        stopFilterPump();
        stateSV3 = false;
        setRelay(RELAY_SV3, false);
        stateSV4 = false;
        setRelay(RELAY_SV4, false);
        stateMainB = false;
        setRelay(RELAY_MAIN_B, false);

        currentGardenZone = 0;
        activeTaskName = "";
        Serial.println("🌱 Stopped Garden Sprinkler.");
    }

    static bool startPoolTopUp(uint8_t poolZone, uint16_t durationMin = 15) {
        StateLock lock;
        if (currentError != ERR_NONE) {
            Serial.printf("❌ Cannot start pool top-up: System in Error State (E%d)\n", currentError);
            return false;
        }

        if (stateBorehole) {
            Serial.println("⚠️ Cannot start pool top-up: Borehole pump running (Interlock)");
            return false;
        }

        // 🛡️ ความปลอดภัยแทงค์น้ำ: หาก Node 3 ขาดการติดต่อ ไม่รู้ระดับน้ำ หรือน้ำในแทงค์ต่ำวิกฤต
        // BUG-3 FIX: ใช้ค่า nodeOfflineTimeoutMin จาก config แทน hardcoded 120s
        unsigned long nodeOfflineTimeoutMs = (configManager.config.nodeOfflineTimeoutMin > 0) ? ((unsigned long)configManager.config.nodeOfflineTimeoutMin * 60000UL) : 120000UL;
        if (lastNode3Time == 0 || millis() - lastNode3Time > nodeOfflineTimeoutMs) {
            Serial.println("❌ Cannot start pool top-up: Node 3 Tank Sensor Offline! Aborting for safety.");
            currentError = ERR_NODE_LOST;
            soundBeep(3, 200);
            return false;
        }
        if (tankData.waterLevelPercent <= configManager.config.tankSafeCutoff && tankData.waterLevelPercent > 0.0f) {
            Serial.println("❌ Cannot start pool top-up: Tank Water Level Critically Low! Aborting.");
            currentError = ERR_TANK_DRY;
            soundBeep(3, 200);
            return false;
        }

        // 🛡️ ตรวจสอบระดับน้ำในสระ: หากน้ำเต็มอยู่แล้ว (ลูกลอยไม่ลด) ห้ามเติมน้ำเด็ดขาดเพื่อป้องกันน้ำล้นสระ
        if (poolZone == 1 && !pool1Data.waterLow) {
            Serial.println("⚠️ Cannot start pool top-up: Wave Pool (Node 1) is already FULL!");
            soundBeep(2, 100);
            return false;
        }
        if (poolZone == 2 && !pool2Data.waterLow) {
            Serial.println("⚠️ Cannot start pool top-up: Play Pool (Node 2) is already FULL!");
            soundBeep(2, 100);
            return false;
        }

        // Step 1: ปิด Main B และโซนรดน้ำก่อน
        stateMainB = false;
        setRelay(RELAY_MAIN_B, false);
        stateSV3 = false;
        setRelay(RELAY_SV3, false);
        stateSV4 = false;
        setRelay(RELAY_SV4, false);

        if (poolZone == 1) {
            stateSV2 = false;
            setRelay(RELAY_SV2, false);
            stateSV1 = true;
            setRelay(RELAY_SV1, true);
        } else if (poolZone == 2) {
            stateSV1 = false;
            setRelay(RELAY_SV1, false);
            stateSV2 = true;
            setRelay(RELAY_SV2, true);
        } else {
            return false;
        }

        // Step 2: เปิด Main Solenoid A (เข้าถังกรองสระ)
        stateMainA = true;
        setRelay(RELAY_MAIN_A, true);
        delay(500);

        // Step 3: สตาร์ทปั๊มดันน้ำ
        startFilterPump();

        currentPoolTaskZone = poolZone;
        poolTaskStartTime = millis();
        poolTaskDurationMs = (unsigned long)durationMin * 60000UL;
        activeTaskName = (poolZone == 1) ? "เติมน้ำสระคลื่น (SV1)" : "เติมน้ำสระเล่น (SV2)";

        Serial.printf("🌊 Started Pool Top-up Zone %d for %d minutes.\n", poolZone, durationMin);
        return true;
    }

    static void stopPoolTopUp() {
        StateLock lock;
        stopFilterPump();
        stateSV1 = false;
        setRelay(RELAY_SV1, false);
        stateSV2 = false;
        setRelay(RELAY_SV2, false);
        stateMainA = false;
        setRelay(RELAY_MAIN_A, false);

        currentPoolTaskZone = 0;
        activeTaskName = "";
        Serial.println("🌊 Stopped Pool Top-up.");
    }
};

#endif // HARDWARE_CONTROLLER_H
