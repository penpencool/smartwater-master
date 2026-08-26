# Smart Water & Irrigation Automation Project

ระบบควบคุมน้ำและสปริงเกลอร์อัตโนมัติพลังงานแสงอาทิตย์ พัฒนาด้วย **PlatformIO (Arduino Framework for ESP32)**

---

## 📁 โครงสร้างโปรเจกต์ (PlatformIO Structure)

```
biggift/
├── common/
│   └── espnow_types.h              <-- Struct ข้อมูลกลางที่ใช้ร่วมกันทุกโหนดผ่าน ESP-NOW
│
├── master_control/                 <-- โปรเจกต์ PlatformIO สำหรับตู้ Master Controller
│   ├── platformio.ini              <-- การตั้งค่า Board esp32doit-devkit-v1, ArduinoJson, Includes
│   ├── include/
│   │   ├── config_manager.h        <-- จัดการบันทึก NVS Preferences
│   │   └── web_dashboard.h         <-- Web UI Dashboard (Responsive Dark Mode)
│   └── src/
│       └── main.cpp                <-- โค้ดหลัก Master Controller (Relays, Flow, LEDs, Buzzer, Web, AP Timer)
│
└── node3_tank/                     <-- โปรเจกต์ PlatformIO สำหรับ Node 3 (แทงค์พักน้ำ)
    ├── platformio.ini              <-- การตั้งค่า Board esp32doit-devkit-v1, Includes
    └── src/
        └── main.cpp                <-- โค้ดหลัก Node 3 (JSN-SR04T Ultrasonic + Float Switch Backup)
```

---

## 🚀 วิธีการเปิดใช้งานและ Build ด้วย PlatformIO (VS Code)

1. **เปิดโฟลเดอร์โหนดที่ต้องการใน VS Code**:
   - สำหรับ Master: เปิดโฟลเดอร์ `master_control`
   - สำหรับ Node 3: เปิดโฟลเดอร์ `node3_tank`
2. **Build / Compile**:
   - กดปุ่ม **Build (✓)** ที่แถบด้านล่างของ PlatformIO หรือใช้คำสั่ง:
     ```bash
     pio run
     ```
3. **Upload เฟิร์มแวร์เข้า ESP32**:
   - เสียบสาย USB เข้า ESP32 แล้วกดปุ่ม **Upload (➔)** หรือใช้คำสั่ง:
     ```bash
     pio run --target upload
     ```
4. **เปิด Serial Monitor**:
   - ใช้ Baud rate `115200` หรือใช้คำสั่ง:
     ```bash
     pio device monitor
     ```

---

## ⚡ คุณสมบัติหลักที่ติดตั้งแล้ว (Phase 1)
- **Master Controller**:
  - รองรับรีเลย์ 8 ช่อง 5V Active HIGH
  - AP Mode เปิด 1 นาทีตอนบูตสำหรับตั้งค่า Wi-Fi แล้วปิดอัตโนมัติ
  - Web UI Dashboard สั่งงานแมนนวลและดูระดับน้ำแทงค์
  - Flow Watchdog ป้องกันปั๊มไหม้
  - Front Panel: `SYS_LED` (Heartbeat), `ERR_LED` (Blink Codes E1-E4), `BUZZER` (Beep Alarm)
- **Node 3 Tank Sensor**:
  - วัดระดับน้ำด้วย JSN-SR04T แปลงเป็น 0-100%
  - Float Switch Hardware Backup ป้องกันน้ำล้น
  - ส่งข้อมูลผ่าน ESP-NOW ทุก 15 วินาที
