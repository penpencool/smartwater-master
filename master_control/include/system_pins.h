#ifndef SYSTEM_PINS_H
#define SYSTEM_PINS_H

#include <Arduino.h>

// ==========================================
// 8-Channel Relay Module (Active HIGH)
// ==========================================
#define RELAY_BOREHOLE       23  // CH1: ปั๊มบาดาล 1,500W (Dry Contact)
#define RELAY_FILTER_ON      22  // CH2: ปั๊มดันน้ำ สั่งเปิด ON (Pulse 400ms สั่ง RF433)
#define RELAY_FILTER_OFF     21  // CH3: ปั๊มดันน้ำ สั่งปิด OFF (Pulse 400ms สั่ง RF433)
#define RELAY_MAIN_A         19  // CH4: Solenoid Main A (เข้ากรองสระ)
#define RELAY_MAIN_B         18  // CH5: Solenoid Main B (Bypass รดน้ำ)
#define RELAY_SV1            5   // CH6: Solenoid SV1 (สระคลื่น - Node 1)
#define RELAY_SV2            4   // CH7: Solenoid SV2 (สระเล่น - Node 2)
#define RELAY_SV3            17  // CH7b: Solenoid SV3 (รดน้ำ Zone 1)
#define RELAY_SV4            16  // CH8: Solenoid SV4 (รดน้ำ Zone 2)

// ==========================================
// Sensor Inputs
// ==========================================
#define PIN_FLOW_SWITCH      34  // Water Flow Switch (Pull-up 10k to 3.3V)

// ==========================================
// Front Panel Indicators & Alarm
// ==========================================
#define PIN_SYS_LED          25  // 🟢 SYS_LED (System Standby / Heartbeat)
#define PIN_ERR_LED          33  // 🔴 ERR_LED (Blink Codes E1 - E5)
#define PIN_BUZZER           32  // 🔊 BUZZER (Active Buzzer Alarm)

#endif // SYSTEM_PINS_H
