#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "espnow_types.h"

// ==========================================
// Thread Safety (FreeRTOS Recursive Mutex)
// ==========================================
extern SemaphoreHandle_t stateMutex;

class StateLock {
public:
    StateLock() {
        if (stateMutex != NULL) {
            xSemaphoreTakeRecursive(stateMutex, portMAX_DELAY);
        }
    }
    ~StateLock() {
        if (stateMutex != NULL) {
            xSemaphoreGiveRecursive(stateMutex);
        }
    }
};

// ==========================================
// System Error Codes
// ==========================================
enum ErrorCode : uint8_t {
    ERR_NONE            = 0,
    ERR_NO_FLOW         = 1, // E1: ปั๊มดันน้ำเดินแต่น้ำไม่ไหล (Flow Timeout)
    ERR_TANK_DRY        = 2, // E2: น้ำในแทงค์ต่ำวิกฤต (<= 25%)
    ERR_NODE_LOST       = 3, // E3: Node 3 แทงค์น้ำออฟไลน์/เซ็นเซอร์ไม่ส่งข้อมูล (ล็อกระบบเพื่อความปลอดภัย)
    ERR_LOW_SOLAR       = 4, // E4: แดดอ่อน / โวลต์ตก
    ERR_NTP_SYNC        = 5, // E5: ซิงค์เวลาจากอินเทอร์เน็ตไม่สำเร็จ
    ERR_ESTOP           = 99 // E-Stop กดยกเลิกฉุกเฉิน
};

extern ErrorCode currentError;
extern String activeTaskName;

// ==========================================
// Relay States (All 8 Channels)
// ==========================================
extern bool stateBorehole;     // CH1: ปั๊มบาดาล
extern unsigned long lastBoreholeStopTime; // เวลาที่ปั๊มบาดาลหยุดล่าสุด (สำหรับคำนวณ Dead-time)
extern bool stateFilterPump;   // CH2/3: ปั๊มดันน้ำ
extern bool stateMainA;        // CH4: เข้ากรองสระ
extern bool stateMainB;        // CH5: Bypass รดน้ำ
extern bool stateSV1;          // CH6: SV1 สระคลื่น
extern bool stateSV2;          // CH7: SV2 สระเล่น
extern bool stateSV3;          // CH7b: SV3 รดน้ำโซน 1
extern bool stateSV4;          // CH8: SV4 รดน้ำโซน 2

// ==========================================
// Node Telemetry Data & Timestamps
// ==========================================
extern PoolNodePayload pool1Data;
extern unsigned long lastNode1Time;
extern String lastNode1TimeStr;
extern bool simPool1Active;
extern bool simPool1Low;

extern PoolNodePayload pool2Data;
extern unsigned long lastNode2Time;
extern String lastNode2TimeStr;
extern bool simPool2Active;
extern bool simPool2Low;

extern TankNodePayload tankData;
extern unsigned long lastNode3Time;
extern String lastNode3TimeStr;
extern bool simTankActive;
extern float simTankLevel;

extern SolarNodePayload solarData;
extern unsigned long lastNode4Time;
extern String lastNode4TimeStr;

// ==========================================
// Active Tasks Tracking
// ==========================================
extern uint8_t currentGardenZone;
extern unsigned long gardenTaskStartTime;
extern unsigned long gardenTaskDurationMs;
extern int lastRunMinuteZ1;
extern int lastRunMinuteZ2;

extern uint8_t currentPoolTaskZone;
extern unsigned long poolTaskStartTime;
extern unsigned long poolTaskDurationMs;
// ==========================================
// Smart Task Queue System (ระบบคิวจัดลำดับความสำคัญ)
// ==========================================
enum TaskType : uint8_t {
    TASK_NONE         = 0,
    TASK_GARDEN_MANUAL = 1, // Priority 1 (สูงสุด): สั่งรดน้ำด่วน (Zone 1/2)
    TASK_GARDEN_AUTO   = 2, // Priority 2: รดน้ำตามตารางเวลา
    TASK_POOL_MANUAL   = 3, // Priority 3: สั่งเติมน้ำสระด่วน (สระคลื่น/สระเล่น)
    TASK_POOL_AUTO     = 4  // Priority 4 (ต่ำสุด): เติมน้ำสระอัตโนมัติตามลูกลอย
};

struct TaskQueueItem {
    TaskType type;
    uint8_t targetZone;     // Zone 1 หรือ 2
    uint16_t durationMin;   // ระยะเวลาทำงาน (นาที)
    String taskName;        // ชื่อแสดงผล
    unsigned long queuedTime;
    unsigned long executeAtMillis; // เวลาที่จะเริ่มทำงาน (กรณีมี Delay)
};

#define MAX_QUEUE_SIZE 8
extern TaskQueueItem taskQueue[MAX_QUEUE_SIZE];
extern uint8_t taskQueueCount;

class TaskQueueManager {
public:
    static bool enqueue(TaskType type, uint8_t zone, uint16_t durationMin, const String &name, unsigned long delayMs = 0) {
        StateLock lock;
        unsigned long targetExecTime = millis() + delayMs;

        // เช็คว่ามีงานชนิดเดียวกันและโซนเดียวกันในคิวอยู่แล้วหรือไม่ (เพื่อไม่อัดคิวซ้ำ)
        for (uint8_t i = 0; i < taskQueueCount; i++) {
            if (taskQueue[i].type == type && taskQueue[i].targetZone == zone) {
                taskQueue[i].durationMin = durationMin; // อัปเดตระยะเวลา
                taskQueue[i].taskName = name;
                if (delayMs == 0) {
                    taskQueue[i].executeAtMillis = millis(); // ปลด delay ถ้ามีคำสั่งใหม่
                }
                return true;
            }
        }

        if (taskQueueCount >= MAX_QUEUE_SIZE) return false;

        taskQueue[taskQueueCount].type = type;
        taskQueue[taskQueueCount].targetZone = zone;
        taskQueue[taskQueueCount].durationMin = durationMin;
        taskQueue[taskQueueCount].taskName = name;
        taskQueue[taskQueueCount].queuedTime = millis();
        taskQueue[taskQueueCount].executeAtMillis = targetExecTime;
        taskQueueCount++;

        // เรียงลำดับคิวตาม Priority (ค่าน้อย = ความสำคัญสูงกว่า)
        for (uint8_t i = 0; i < taskQueueCount - 1; i++) {
            for (uint8_t j = i + 1; j < taskQueueCount; j++) {
                if ((uint8_t)taskQueue[i].type > (uint8_t)taskQueue[j].type) {
                    TaskQueueItem temp = taskQueue[i];
                    taskQueue[i] = taskQueue[j];
                    taskQueue[j] = temp;
                }
            }
        }
        return true;
    }

    static bool isQueued(TaskType type, uint8_t zone) {
        StateLock lock;
        for (uint8_t i = 0; i < taskQueueCount; i++) {
            if (taskQueue[i].type == type && taskQueue[i].targetZone == zone) {
                return true;
            }
        }
        return false;
    }

    static bool remove(TaskType type, uint8_t zone) {
        StateLock lock;
        for (uint8_t i = 0; i < taskQueueCount; i++) {
            if (taskQueue[i].type == type && taskQueue[i].targetZone == zone) {
                for (uint8_t j = i; j < taskQueueCount - 1; j++) {
                    taskQueue[j] = taskQueue[j + 1];
                }
                taskQueueCount--;
                return true;
            }
        }
        return false;
    }

    static bool popReadyHighest(TaskQueueItem &outItem) {
        StateLock lock;
        if (taskQueueCount == 0) return false;

        unsigned long now = millis();
        // ค้นหางานที่ Priority สูงสุดที่ถึงเวลาทำงานแล้ว (executeAtMillis <= now)
        for (uint8_t i = 0; i < taskQueueCount; i++) {
            if (now >= taskQueue[i].executeAtMillis) {
                outItem = taskQueue[i];
                for (uint8_t j = i; j < taskQueueCount - 1; j++) {
                    taskQueue[j] = taskQueue[j + 1];
                }
                taskQueueCount--;
                return true;
            }
        }
        return false;
    }

    static bool popAtIndex(uint8_t index, TaskQueueItem &outItem) {
        StateLock lock;
        if (index >= taskQueueCount) return false;
        outItem = taskQueue[index];
        for (uint8_t j = index; j < taskQueueCount - 1; j++) {
            taskQueue[j] = taskQueue[j + 1];
        }
        taskQueueCount--;
        return true;
    }

    static void clear() {
        StateLock lock;
        taskQueueCount = 0;
    }
};

extern bool isManualPoolTask;
extern unsigned long poolLowStartTimeWave;
extern unsigned long poolLowStartTimePlay;

extern unsigned long filterPumpStartTime;
extern bool flowWatchdogActive;

// NTP Status
extern bool ntpSynced;

#endif // SYSTEM_STATE_H
