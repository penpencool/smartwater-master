#ifndef ESPNOW_TYPES_H
#define ESPNOW_TYPES_H

#include <Arduino.h>

// Node Identifiers
enum NodeType : uint8_t {
    NODE_WAVE_POOL       = 1, // สระคลื่น
    NODE_PLAY_POOL       = 2, // สระเล่น
    NODE_WATER_TANK      = 3, // แทงค์พักน้ำ 3,000L x 2
    NODE_SOLAR_PANEL     = 4, // โซล่าเซลล์ & แสงแดด
    NODE_TANK_CALIBRATE  = 13 // คำสั่งตั้งค่า Calibration ส่งจาก Master -> Node 3
};

// Master -> Node 3 Calibration Payload
struct __attribute__((packed)) TankCalibrationPayload {
    uint8_t msgType;           // NODE_TANK_CALIBRATE (13)
    float distEmptyCm;         // ระยะแทงค์แห้ง (0%) เช่น 180 cm
    float distFullCm;          // ระยะแทงค์เต็ม (100%) เช่น 25 cm
    uint32_t commandId;
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
