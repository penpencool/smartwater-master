#ifndef ESPNOW_TYPES_H
#define ESPNOW_TYPES_H

#include <Arduino.h>

// Node Identifiers
enum NodeType : uint8_t {
    NODE_WAVE_POOL       = 1, // สระคลื่น
    NODE_PLAY_POOL       = 2, // สระเล่น
    NODE_WATER_TANK      = 3, // แทงค์พักน้ำ 3,000L x 2
    NODE_SOLAR_PANEL     = 4, // โซล่าเซลล์ & แสงแดด
    NODE_TANK_CALIBRATE  = 13, // คำสั่งตั้งค่า Calibration ส่งจาก Master -> Node 3
    NODE_TANK_SYNC_CFG   = 14  // คำสั่ง Sync เวลา & การตั้งค่า Sleep 2 ระดับ จาก Master -> Node 3
};

// Master -> Node 3 Calibration Payload
struct __attribute__((packed)) TankCalibrationPayload {
    uint8_t msgType;           // NODE_TANK_CALIBRATE (13)
    float distEmptyCm;         // ระยะแทงค์แห้ง (0%) เช่น 180 cm
    float distFullCm;          // ระยะแทงค์เต็ม (100%) เช่น 25 cm
    uint32_t commandId;
};

// Master -> Node 3 Sync & Timing Configuration Payload
struct __attribute__((packed)) TankSyncConfigPayload {
    uint8_t msgType;              // NODE_TANK_SYNC_CFG (14)
    uint8_t currentHour;          // เวลาปัจจุบัน Master (ชั่วโมง 0-23)
    uint8_t currentMin;           // เวลาปัจจุบัน Master (นาที 0-59)
    uint8_t currentSec;           // เวลาปัจจุบัน Master (วินาที 0-59)
    bool isNtpSynced;             // true = เวลา Master ซิงค์อินเทอร์เน็ตสมบูรณ์
    
    // ช่วงเวลาทำงานและการ Sleep นอกเวลา
    bool sleepScheduleEnabled;    // เปิด/ปิด การ Sleep นอกเวลาทำงาน (เช่น 18:00 - 06:00)
    uint8_t activeStartHour;      // เริ่มทำงาน (เช่น 6 = 06:00)
    uint8_t activeStartMin;       // เริ่มทำงาน (เช่น 0 = :00)
    uint8_t activeEndHour;        // พัก/เริ่มหลับ (เช่น 18 = 18:00)
    uint8_t activeEndMin;         // พัก/เริ่มหลับ (เช่น 0 = :00)
    
    // การส่งข้อมูล 2 ระดับ (2-Stage Adaptive Sampling)
    uint16_t normalIntervalSec;   // ความถี่ส่งช่วงปกติ (เช่น 30 วินาที)
    uint16_t fastIntervalSec;     // ความถี่ส่งช่วงใกล้เต็ม/เติมน้ำ (เช่น 3 วินาที)
    float fastThresholdPct;       // ระดับน้ำที่ให้เริ่มส่งถี่ (เช่น 80.0%)
    bool isBoreholeRunning;       // ปั๊มบาดาลเปิดอยู่หรือไม่ (ถ้าเปิดอยู่ ให้ Node 3 ส่งถี่ทันที)
    
    uint32_t syncId;              // Sequence ID
};

// Node 3 (Water Tank Payload)
struct __attribute__((packed)) TankNodePayload {
    uint8_t nodeId;            // NODE_WATER_TANK (3)
    float waterLevelPercent;   // 0.0 - 100.0%
    float distanceCm;          // ระยะผิวน้ำที่อ่านได้จาก JSN-SR04T (cm)
    bool floatBackupActive;    // true = ลูกลอยแตะระดับตัด (High level overflow backup)
    float batteryVoltage;      // แรงดันแบตเตอรี่โหนด (V)
    float currentEmptyCm;      // ค่าที่บันทึกอยู่ใน NVS ปัจจุบัน (Empty)
    float currentFullCm;       // ค่าที่บันทึกอยู่ใน NVS ปัจจุบัน (Full)
    uint32_t messageId;        // ลำดับแพ็กเก็ต
};

// Node 1 & 2 (Pool Float Switch Payload)
struct __attribute__((packed)) PoolNodePayload {
    uint8_t nodeId;            // NODE_WAVE_POOL (1) หรือ NODE_PLAY_POOL (2)
    bool waterLow;             // true = ลูกลอยตก (ระดับน้ำลด ต้องการเติม)
    float batteryVoltage;      // แรงดันแบตเตอรี่ (V)
    uint32_t messageId;
};

// Node 4 (Solar Monitor Payload)
struct __attribute__((packed)) SolarNodePayload {
    uint8_t nodeId;            // NODE_SOLAR_PANEL (4)
    float voltageDC;           // แรงดันแผงโซล่าเซลล์ (V)
    float currentDC;           // กระแสแผง (A)
    float powerWatt;           // กำลังไฟฟ้าจริง (W)
    float lightLux;            // ความสว่างจาก LDR/BH1750 (Lux)
    float solarQualityScore;   // ประสิทธิภาพแสงแดด (0 - 100%)
    uint32_t messageId;
};

#endif // ESPNOW_TYPES_H
