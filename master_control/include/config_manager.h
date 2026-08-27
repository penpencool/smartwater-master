#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "system_state.h"

struct ScheduleSlot {
    bool enabled;       // เปิด/ปิด การทำงาน
    uint8_t zone;       // 1 = Zone 1 (SV3), 2 = Zone 2 (SV4)
    uint8_t startHour;  // ชั่วโมงเริ่ม (0-23)
    uint8_t startMin;   // นาทีเริ่ม (0-59)
    uint8_t endHour;    // ชั่วโมงจบ (0-23)
    uint8_t endMin;     // นาทีจบ (0-59)
};

#define MAX_SCHEDULE_SLOTS 6

struct SystemConfig {
    // Tank Level Thresholds (%) & Auto Borehole
    bool autoBoreholeEnabled;   // เปิด/ปิด ระบบเติมน้ำบาดาลอัตโนมัติ
    float tankLowTrigger;       // สั่งเปิดบาดาลเมื่อน้ำ < ค่านี้ (เช่น 70%)
    float tankFullStop;         // สั่งปิดบาดาลเมื่อน้ำ >= ค่านี้ (เช่น 95%)
    float tankSafeCutoff;       // ตัดปั๊มดันน้ำหากน้ำ < ค่านี้ (เช่น 25%)
    float tankEmptyCm;          // ระยะแทงค์แห้ง 0% (เช่น 180.0 cm)
    float tankFullCm;           // ระยะแทงค์เต็ม 100% (เช่น 25.0 cm)

    // Dynamic Garden Schedules (Smart Home style Start Time - End Time)
    uint8_t scheduleCount;
    ScheduleSlot schedules[MAX_SCHEDULE_SLOTS];

    // Pool Auto Top-up Parameters (Wave Pool & Play Pool)
    bool autoPoolWaveEnabled;    // เปิด/ปิด เติมอัตโนมัติสระคลื่น (Node 1)
    uint8_t poolModeWave;        // 0 = เติมตามเวลาที่ตั้ง (เต็มก่อนตัด), 1 = เติมจนกว่าจะถึงระดับเต็ม (ไม่จำกัดรอบ)
    uint16_t poolWaveDelayMin;   // หน่วงเวลากี่นาทีก่อนเติม (เช่น 5 นาที)
    uint16_t poolWaveDurationMin;// ระยะเวลาเติมน้ำ (เช่น 15 นาที)

    bool autoPoolPlayEnabled;    // เปิด/ปิด เติมอัตโนมัติสระเล่น (Node 2)
    uint8_t poolModePlay;        // 0 = เติมตามเวลาที่ตั้ง (เต็มก่อนตัด), 1 = เติมจนกว่าจะถึงระดับเต็ม (ไม่จำกัดรอบ)
    uint16_t poolPlayDelayMin;   // หน่วงเวลากี่นาทีก่อนเติม (เช่น 5 นาที)
    uint16_t poolPlayDurationMin;// ระยะเวลาเติมน้ำ (เช่น 15 นาที)

    // Solar Parameters
    float solarMinVolt;         // โวลต์แผงขั้นต่ำที่พร้อมทำงาน (เช่น 120V)
    uint8_t solarStartHour;     // 8 (08:00)
    uint8_t solarEndHour;       // 16 (16:30)

    // Safety & Timing
    uint16_t flowTimeoutSec;        // Flow switch confirmation timeout (เช่น 8s)
    uint16_t deadTimeSec;           // Dead-time ก่อนสลับปั๊ม (เช่น 10s)
    uint16_t poolTopUpDelayMin;     // เติมน้ำต่อหลังลูกลอยยกตัว (เช่น 5 นาที)
    uint16_t nodeOfflineTimeoutMin; // เซ็นเซอร์ไม่ส่งข้อมูลเกินกี่นาที ถึงจะนับว่า Offline (เช่น 3 นาที)

    // Wi-Fi Config
    char wifiSSID[32];
    char wifiPassword[64];

    // GitHub OTA Config
    char githubRepo[64]; // เช่น "penpencool/smartwater-master"
};

class ConfigManager {
private:
    Preferences prefs;

public:
    SystemConfig config;

    void begin() {
        prefs.begin("smartwater", false);
        load();
    }

    void load() {
        StateLock lock;
        if (!prefs.isKey("tankLow")) {
            // First time boot: save defaults into NVS
            saveDefaults();
        }

        config.autoBoreholeEnabled = prefs.getBool("autoBorehole", true);
        config.tankLowTrigger   = prefs.getFloat("tankLow", 70.0f);
        config.tankFullStop     = prefs.getFloat("tankFull", 95.0f);
        config.tankSafeCutoff   = prefs.getFloat("tankSafe", 25.0f);
        config.tankEmptyCm      = prefs.getFloat("tankEmpty", 180.0f);
        config.tankFullCm       = prefs.getFloat("tankFull_cm", 25.0f);

        // Load Dynamic Schedules
        config.scheduleCount = prefs.getUChar("schedCount", 2);
        if (config.scheduleCount > MAX_SCHEDULE_SLOTS) config.scheduleCount = MAX_SCHEDULE_SLOTS;

        for (uint8_t i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
            String prefix = "sc_" + String(i) + "_";
            config.schedules[i].enabled   = prefs.getBool((prefix + "en").c_str(), (i == 0 || i == 1));
            config.schedules[i].zone      = prefs.getUChar((prefix + "z").c_str(), (i == 0 ? 1 : 2));
            config.schedules[i].startHour = prefs.getUChar((prefix + "sh").c_str(), (i == 0 ? 6 : 17));
            config.schedules[i].startMin  = prefs.getUChar((prefix + "sm").c_str(), (i == 0 ? 30 : 0));
            config.schedules[i].endHour   = prefs.getUChar((prefix + "eh").c_str(), (i == 0 ? 6 : 17));
            config.schedules[i].endMin    = prefs.getUChar((prefix + "em").c_str(), (i == 0 ? 45 : 15));
        }

        // Pool Top-up Configs
        config.autoPoolWaveEnabled    = prefs.getBool("autoPWave", true);
        config.poolModeWave           = prefs.getUChar("pWaveMode", 0);
        config.poolWaveDelayMin       = prefs.getUShort("pWaveDly", 5);
        config.poolWaveDurationMin    = prefs.getUShort("pWaveDur", 15);

        config.autoPoolPlayEnabled    = prefs.getBool("autoPPlay", true);
        config.poolModePlay           = prefs.getUChar("pPlayMode", 0);
        config.poolPlayDelayMin       = prefs.getUShort("pPlayDly", 5);
        config.poolPlayDurationMin    = prefs.getUShort("pPlayDur", 15);

        config.solarMinVolt     = prefs.getFloat("solarMinV", 120.0f);
        config.solarStartHour   = prefs.getUChar("solarStartH", 8);
        config.solarEndHour     = prefs.getUChar("solarEndH", 16);

        config.flowTimeoutSec   = prefs.getUShort("flowTimeout", 8);
        config.deadTimeSec      = prefs.getUShort("deadTime", 10);
        config.poolTopUpDelayMin= prefs.getUShort("poolDelay", 5);
        config.nodeOfflineTimeoutMin = prefs.getUShort("nodeOffMin", 2);

        String ssid = prefs.getString("ssid", "");
        String pass = prefs.getString("pass", "");
        strncpy(config.wifiSSID, ssid.c_str(), sizeof(config.wifiSSID) - 1);
        config.wifiSSID[sizeof(config.wifiSSID) - 1] = '\0';
        strncpy(config.wifiPassword, pass.c_str(), sizeof(config.wifiPassword) - 1);
        config.wifiPassword[sizeof(config.wifiPassword) - 1] = '\0';

        String repo = prefs.getString("ghRepo", "penpencool/smartwater-master");
        strncpy(config.githubRepo, repo.c_str(), sizeof(config.githubRepo) - 1);
        config.githubRepo[sizeof(config.githubRepo) - 1] = '\0';
    }

    void saveDefaults() {
        StateLock lock;
        config.autoBoreholeEnabled = true;
        config.tankLowTrigger = 70.0f;
        config.tankFullStop = 95.0f;
        config.tankSafeCutoff = 25.0f;
        config.tankEmptyCm = 180.0f;
        config.tankFullCm = 25.0f;

        config.scheduleCount = 2;
        // Default Slot 0: Zone 1 (06:30 - 06:45)
        config.schedules[0] = {true, 1, 6, 30, 6, 45};
        // Default Slot 1: Zone 2 (17:00 - 17:15)
        config.schedules[1] = {true, 2, 17, 0, 17, 15};
        for (uint8_t i = 2; i < MAX_SCHEDULE_SLOTS; i++) {
            config.schedules[i] = {false, 1, 8, 0, 8, 15};
        }

        config.autoPoolWaveEnabled = true;
        config.poolModeWave = 0;
        config.poolWaveDelayMin = 5;
        config.poolWaveDurationMin = 15;

        config.autoPoolPlayEnabled = true;
        config.poolModePlay = 0;
        config.poolPlayDelayMin = 5;
        config.poolPlayDurationMin = 15;

        config.solarMinVolt = 120.0f;
        config.solarStartHour = 8;
        config.solarEndHour = 16;
        config.flowTimeoutSec = 8;
        config.deadTimeSec = 10;
        config.poolTopUpDelayMin = 5;
        config.nodeOfflineTimeoutMin = 2;
        memset(config.wifiSSID, 0, sizeof(config.wifiSSID));
        memset(config.wifiPassword, 0, sizeof(config.wifiPassword));
        strncpy(config.githubRepo, "penpencool/smartwater-master", sizeof(config.githubRepo) - 1);
        config.githubRepo[sizeof(config.githubRepo) - 1] = '\0';
        save();
    }

    void save() {
        StateLock lock;
        prefs.putBool("autoBorehole", config.autoBoreholeEnabled);
        prefs.putFloat("tankLow", config.tankLowTrigger);
        prefs.putFloat("tankFull", config.tankFullStop);
        prefs.putFloat("tankSafe", config.tankSafeCutoff);
        prefs.putFloat("tankEmpty", config.tankEmptyCm);
        prefs.putFloat("tankFull_cm", config.tankFullCm);

        prefs.putUChar("schedCount", config.scheduleCount);
        for (uint8_t i = 0; i < MAX_SCHEDULE_SLOTS; i++) {
            String prefix = "sc_" + String(i) + "_";
            prefs.putBool((prefix + "en").c_str(), config.schedules[i].enabled);
            prefs.putUChar((prefix + "z").c_str(), config.schedules[i].zone);
            prefs.putUChar((prefix + "sh").c_str(), config.schedules[i].startHour);
            prefs.putUChar((prefix + "sm").c_str(), config.schedules[i].startMin);
            prefs.putUChar((prefix + "eh").c_str(), config.schedules[i].endHour);
            prefs.putUChar((prefix + "em").c_str(), config.schedules[i].endMin);
        }

        prefs.putBool("autoPWave", config.autoPoolWaveEnabled);
        prefs.putUChar("pWaveMode", config.poolModeWave);
        prefs.putUShort("pWaveDly", config.poolWaveDelayMin);
        prefs.putUShort("pWaveDur", config.poolWaveDurationMin);

        prefs.putBool("autoPPlay", config.autoPoolPlayEnabled);
        prefs.putUChar("pPlayMode", config.poolModePlay);
        prefs.putUShort("pPlayDly", config.poolPlayDelayMin);
        prefs.putUShort("pPlayDur", config.poolPlayDurationMin);

        prefs.putFloat("solarMinV", config.solarMinVolt);
        prefs.putUChar("solarStartH", config.solarStartHour);
        prefs.putUChar("solarEndH", config.solarEndHour);

        prefs.putUShort("flowTimeout", config.flowTimeoutSec);
        prefs.putUShort("deadTime", config.deadTimeSec);
        prefs.putUShort("poolDelay", config.poolTopUpDelayMin);
        prefs.putUShort("nodeOffMin", config.nodeOfflineTimeoutMin);

        prefs.putString("ssid", config.wifiSSID);
        prefs.putString("pass", config.wifiPassword);
        prefs.putString("ghRepo", config.githubRepo);
    }
};

extern ConfigManager configManager;

#endif // CONFIG_MANAGER_H
