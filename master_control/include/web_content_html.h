#ifndef WEB_CONTENT_HTML_H
#define WEB_CONTENT_HTML_H

#include <Arduino.h>

const char DASHBOARD_BODY[] PROGMEM = R"rawliteral(
  <!-- Toast Notification Label -->
  <div id="notifyToast"><span id="notifyIcon">ℹ️</span><span id="notifyText">ข้อความแจ้งเตือน</span></div>

  <!-- Duration Custom Action Modal -->
  <div id="durModal">
    <div class="modal-box">
      <div id="durModalTitle" class="modal-title">ระบุระยะเวลาทำงาน</div>
      <div class="input-row">
        <label id="durModalLabel">ระยะเวลา (นาที):</label>
        <input type="number" id="durInput" min="1" max="180" value="15">
      </div>
      <div style="display: flex; gap: 10px; margin-top: 18px;">
        <button class="btn" style="flex: 1;" onclick="closeDurModal()">ยกเลิก</button>
        <button class="btn primary" style="flex: 1;" id="durConfirmBtn">ยืนยัน</button>
      </div>
    </div>
  </div>

  <!-- Top Header -->
  <div class="header">
    <h1>💧 Smart Water Master</h1>
    <div style="display: flex; gap: 8px; align-items: center; flex-wrap: wrap;">
      <div id="timeBadge" class="status-badge" style="background: rgba(30, 41, 59, 0.8); border: 1px solid var(--accent-cyan); color: var(--accent-cyan); font-family: monospace; font-size: 0.95rem;">🕒 --:--:--</div>
      <div id="sysBadge" class="status-badge">🟢 ระบบปกติ (Idle)</div>
    </div>
  </div>

  <!-- Startup / Diagnostics Alert Banner -->
  <div id="alertBanner">
    <div style="display: flex; align-items: center; gap: 10px; flex: 1;">
      <span id="alertIcon">⚠️</span>
      <span id="alertMsg">กำลังตรวจสอบระบบ...</span>
    </div>
    <button id="alertResetBtn" class="btn" style="background: rgba(16, 185, 129, 0.25); border: 1px solid #10b981; color: #10b981; padding: 6px 14px; font-size: 0.85rem; font-weight: 600; display: none;" onclick="sendCommand('reset_error')">
      🟢 ปลดล็อก / รีเซ็ต Alarm
    </button>
  </div>

  <!-- Category Navigation Tabs -->
  <div class="tab-nav">
    <button class="tab-btn active" onclick="switchTab('tab-overview')">📊 ภาพรวม & ควบคุม</button>
    <button class="tab-btn" onclick="switchTab('tab-garden')">🌱 ระบบรดน้ำ & ตั้งเวลา</button>
    <button class="tab-btn" onclick="switchTab('tab-pool')">🏊 สระว่ายน้ำ & เติมอัตโนมัติ</button>
    <button class="tab-btn" onclick="switchTab('tab-tank')">💧 แทงค์น้ำ & ปั๊มบาดาล</button>
    <button class="tab-btn" onclick="switchTab('tab-system')">⚙️ พลังงาน & ตั้งค่าระบบ</button>
  </div>

  <!-- ================= TAB 1: OVERVIEW ================= -->
  <div id="tab-overview" class="tab-pane active">
    <div class="grid">
      <!-- Big Hero Status Card: แสดงสถานะการทำงานปัจจุบันแบบชัดเจนเป็นอันดับแรก -->
      <div class="card" style="grid-column: span 2; background: linear-gradient(135deg, rgba(15, 23, 42, 0.9), rgba(30, 41, 59, 0.7)); border: 1.5px solid #38bdf8; box-shadow: 0 4px 20px rgba(2, 132, 199, 0.2);">
        <div style="display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 12px;">
          <div>
            <div style="font-size: 0.85rem; color: #94a3b8; font-weight: 500; margin-bottom: 4px;">⚡ สถานะการทำงานปัจจุบัน (System Activity)</div>
            <div id="mainHeroStatusText" style="font-size: 1.4rem; font-weight: 700; color: #34d399; display: flex; align-items: center; gap: 10px;">
              <span>🟢 ระบบว่าง (Standby / Idle)</span>
            </div>
            <div id="mainHeroSubText" style="font-size: 0.85rem; color: var(--text-muted); margin-top: 4px;">
              พร้อมรับคำสั่งงานด่วน หรือทำงานตามตารางอัตโนมัติ
            </div>
          </div>
          <div id="mainHeroTimerBadge" style="display: none; background: rgba(2, 132, 199, 0.25); border: 1.5px solid #38bdf8; padding: 10px 18px; border-radius: 14px; text-align: right;">
            <div style="font-size: 0.75rem; color: #38bdf8; font-weight: 600;">เหลือเวลาทำงาน</div>
            <div id="mainHeroTimerVal" style="font-size: 1.5rem; font-weight: 800; font-family: monospace; color: white;">
              ⏱️ 00:00
            </div>
          </div>
        </div>

        <!-- Live Queue Indicator List -->
        <div id="heroQueueContainer" style="display: none; margin-top: 14px; padding-top: 12px; border-top: 1px dashed rgba(255,255,255,0.15);">
          <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
            <span style="font-size: 0.85rem; font-weight: 600; color: #fbbf24;">📋 คิวงานรอทำต่อ (Waiting Queue):</span>
            <span id="heroQueueCountBadge" style="font-size: 0.75rem; background: rgba(245, 158, 11, 0.2); color: #fbbf24; border: 1px solid rgba(245, 158, 11, 0.4); padding: 2px 8px; border-radius: 10px;">0 งาน</span>
          </div>
          <div id="heroQueueList" style="display: flex; flex-direction: column; gap: 6px;"></div>
        </div>
      </div>

      <!-- Tank Overview Card -->
      <div class="card">
        <div class="card-title">
          <span>แทงค์พักน้ำ 6,000L (Node 3)</span>
          <span id="node3Status" style="font-size: 0.8rem; color: #ef4444;">● Offline</span>
        </div>
        <div class="tank-container">
          <div class="tank-graphic">
            <div id="tankLevelBar" class="tank-water" style="height: 0%;"></div>
          </div>
          <div>
            <div id="tankLevelText" class="tank-value">-- %</div>
            <div id="tankDistText" class="tank-sub">ระยะวัด: รอข้อมูล...</div>
            <div id="tankFloatText" class="tank-sub" style="margin-top: 4px;">ลูกลอย Backup: ไม่ทราบสถานะ</div>
            <div id="node3LastTime" class="tank-sub" style="margin-top: 4px; color: var(--accent-cyan);">🕒 รอรับข้อมูล...</div>
          </div>
        </div>
      </div>

      <!-- Pools Status Card (Node 1 & Node 2) -->
      <div class="card">
        <div class="card-title">
          <span>ระดับน้ำสระว่ายน้ำ (Pool Nodes)</span>
        </div>
        <div style="background: rgba(15, 23, 42, 0.4); padding: 12px; border-radius: 10px; margin-bottom: 10px; border: 1px solid var(--card-border);">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:4px;">
            <strong style="color: var(--accent-cyan);">🌊 สระคลื่น (Wave Pool - Node 1)</strong>
            <span id="node1Badge" style="font-size: 0.8rem; color: #ef4444;">● Offline</span>
          </div>
          <div style="display:flex; justify-content:space-between; font-size: 0.85rem;">
            <span>สถานะลูกลอย: <strong id="node1FloatTxt">รอข้อมูล...</strong></span>
            <span id="node1BatTxt" style="color: var(--text-muted);">-- V</span>
          </div>
          <div style="font-size: 0.75rem; color: var(--text-muted); margin-top: 4px;" id="node1LastTime">
            🕒 รอรับข้อมูล...
          </div>
        </div>

        <div style="background: rgba(15, 23, 42, 0.4); padding: 12px; border-radius: 10px; border: 1px solid var(--card-border);">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:4px;">
            <strong style="color: #38bdf8;">🏊 สระเล่น (Play Pool - Node 2)</strong>
            <span id="node2Badge" style="font-size: 0.8rem; color: #ef4444;">● Offline</span>
          </div>
          <div style="display:flex; justify-content:space-between; font-size: 0.85rem;">
            <span>สถานะลูกลอย: <strong id="node2FloatTxt">รอข้อมูล...</strong></span>
            <span id="node2BatTxt" style="color: var(--text-muted);">-- V</span>
          </div>
          <div style="font-size: 0.75rem; color: var(--text-muted); margin-top: 4px;" id="node2LastTime">
            🕒 รอรับข้อมูล...
          </div>
        </div>
      </div>

      <!-- Quick Manual Override -->
      <div class="card">
        <div class="card-title">สั่งงานด่วน (Manual Override)</div>

        <!-- Task Live Countdown Timer Pill -->
        <div id="taskCountdownBox" style="display: none; background: rgba(2, 132, 199, 0.2); border: 1px solid var(--accent-cyan); padding: 10px 14px; border-radius: 12px; margin-bottom: 12px; align-items: center; justify-content: space-between;">
          <div style="font-size: 0.85rem; color: var(--accent-cyan); font-weight: 500;">
            <span id="taskCountdownLabel">🔵 กำลังทำงาน:</span>
          </div>
          <div style="font-size: 1.1rem; font-weight: 700; color: white; font-family: monospace;" id="taskCountdownVal">
            ⏱️ 00:00
          </div>
        </div>

        <!-- Borehole Pump Dedicated Toggle Switch -->
        <div style="background: rgba(15, 23, 42, 0.5); padding: 12px 16px; border-radius: 12px; margin-bottom: 12px; border: 1px solid var(--card-border); display: flex; justify-content: space-between; align-items: center;">
          <div>
            <div style="font-weight: 600; font-size: 0.95rem; display: flex; align-items: center; gap: 8px;">
              <span>🚰 ปั๊มบาดาล 1,500W</span>
              <span id="boreholeToggleStatus" style="font-size: 0.8rem; color: #94a3b8; font-weight: normal;">(ปิด OFF)</span>
            </div>
            <div style="font-size: 0.75rem; color: var(--text-muted); margin-top: 2px;">
              สั่งเปิด/ปิดแมนนวล หรือจะตัดปิดอัตโนมัติเมื่อน้ำถึง % ที่กำหนด
            </div>
          </div>
          <label class="switch">
            <input type="checkbox" id="boreholeToggleSwitch" onchange="toggleBoreholeSwitch(this.checked)">
            <span class="slider"></span>
          </label>
        </div>

        <div class="btn-group">
          <button class="btn primary" onclick="sendPoolCommand('wave')">🌊 เติมสระคลื่น (SV1)</button>
          <button class="btn primary" onclick="sendPoolCommand('play')">🏊 เติมสระเล่น (SV2)</button>
          <button class="btn primary" onclick="sendGardenCommand(1)">🌱 รดน้ำ Zone 1 (SV3)</button>
          <button class="btn primary" onclick="sendGardenCommand(2)">🌱 รดน้ำ Zone 2 (SV4)</button>
          <button class="btn success" style="grid-column: span 1;" onclick="sendCommand('reset_error')">🟢 ปลดล็อก / รีเซ็ต Alarm</button>
          <button class="btn" style="background: rgba(239, 68, 68, 0.2); border: 1px solid #ef4444; color: #ef4444;" onclick="sendCommand('stop_all')">⏹️ หยุดทั้งหมด</button>
          <button class="btn danger" style="grid-column: span 2;" onclick="sendCommand('emergency_stop')">🚨 EMERGENCY STOP (ล็อกระบบ)</button>
        </div>
      </div>

      <!-- Relays Status Card (Full 8 Relays) -->
      <div class="card" style="grid-column: span 1;">
        <div class="card-title">สถานะอุปกรณ์ควบคุม (Relays ครบทุกตัว)</div>
        <div class="relay-item">
          <span><span id="r1_tag" class="relay-tag"></span>CH1: ปั๊มบาดาล 1,500W (Inverter)</span>
          <strong id="r1_txt">OFF</strong>
        </div>
        <div class="relay-item">
          <span><span id="r2_tag" class="relay-tag"></span>CH2/3: ปั๊มดันกรองน้ำ (RF433 Pulse)</span>
          <strong id="r2_txt">OFF</strong>
        </div>
        <div class="relay-item">
          <span><span id="r4_tag" class="relay-tag"></span>CH4: Solenoid Main A (เข้ากรองสระ)</span>
          <strong id="r4_txt">CLOSED</strong>
        </div>
        <div class="relay-item">
          <span><span id="r5_tag" class="relay-tag"></span>CH5: Solenoid Main B (Bypass รดน้ำ)</span>
          <strong id="r5_txt">CLOSED</strong>
        </div>
        <div class="relay-item">
          <span><span id="r6_tag" class="relay-tag"></span>CH6: Solenoid SV1 (สระคลื่น)</span>
          <strong id="r6_txt">CLOSED</strong>
        </div>
        <div class="relay-item">
          <span><span id="r7_tag" class="relay-tag"></span>CH7: Solenoid SV2 (สระเล่น)</span>
          <strong id="r7_txt">CLOSED</strong>
        </div>
        <div class="relay-item">
          <span><span id="r7b_tag" class="relay-tag"></span>CH7b: Solenoid SV3 (รดน้ำ Zone 1)</span>
          <strong id="r7b_txt">CLOSED</strong>
        </div>
        <div class="relay-item">
          <span><span id="r8_tag" class="relay-tag"></span>CH8: Solenoid SV4 (รดน้ำ Zone 2)</span>
          <strong id="r8_txt">CLOSED</strong>
        </div>
      </div>
    </div>
  </div>

  <!-- ================= TAB 2: GARDEN SCHEDULER ================= -->
  <div id="tab-garden" class="tab-pane">
    <div class="grid">
      <!-- Garden Water Thresholds Config Card -->
      <div class="card" style="grid-column: span 2; border: 1.5px solid rgba(52, 211, 153, 0.4); background: linear-gradient(135deg, rgba(15, 23, 42, 0.85), rgba(6, 78, 59, 0.25));">
        <div class="card-title">
          <span>💧 เกณฑ์ระดับน้ำในแทงค์สำหรับการรดน้ำ (Water Thresholds)</span>
          <span style="font-size: 0.8rem; color: #34d399;">แยกอิสระแต่ละโซน</span>
        </div>
        <div style="font-size: 0.82rem; color: var(--text-muted); margin-bottom: 14px;">
          กำหนดระดับน้ำในแทงค์ (%) ที่ต้องมีก่อนเริ่มรดน้ำ และหากรดน้ำอยู่น้ำลดต่ำกว่าเกณฑ์หยุด ระบบจะพักงานไว้ ปั๊มบาดาลจะเติมน้ำจนถึงเกณฑ์เริ่ม จึงจะกลับมารดน้ำต่อตามเวลาที่เหลือโดยอัตโนมัติ
        </div>
        <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px;">
          <!-- Zone 1 Thresholds -->
          <div style="background: rgba(15, 23, 42, 0.6); padding: 14px; border-radius: 10px; border: 1px solid rgba(52, 211, 153, 0.3);">
            <div style="font-weight: 700; color: #34d399; margin-bottom: 10px; display: flex; align-items: center; gap: 6px;">
              <span>🌿 โซน 1 (SV3)</span>
            </div>
            <div class="input-row">
              <label>ระดับน้ำขั้นต่ำที่จะเริ่มรด (%):</label>
              <input type="number" id="gZ1Start" min="5" max="100" placeholder="เช่น 50 (%)">
            </div>
            <div class="input-row">
              <label>ระดับน้ำที่จะตัดหยุดพัก (%):</label>
              <input type="number" id="gZ1Stop" min="5" max="95" placeholder="เช่น 25 (%)">
            </div>
          </div>

          <!-- Zone 2 Thresholds -->
          <div style="background: rgba(15, 23, 42, 0.6); padding: 14px; border-radius: 10px; border: 1px solid rgba(52, 211, 153, 0.3);">
            <div style="font-weight: 700; color: #34d399; margin-bottom: 10px; display: flex; align-items: center; gap: 6px;">
              <span>🌿 โซน 2 (SV4)</span>
            </div>
            <div class="input-row">
              <label>ระดับน้ำขั้นต่ำที่จะเริ่มรด (%):</label>
              <input type="number" id="gZ2Start" min="5" max="100" placeholder="เช่น 50 (%)">
            </div>
            <div class="input-row">
              <label>ระดับน้ำที่จะตัดหยุดพัก (%):</label>
              <input type="number" id="gZ2Stop" min="5" max="95" placeholder="เช่น 25 (%)">
            </div>
          </div>
        </div>
        <button class="btn primary" style="width: 100%; margin-top: 14px; padding: 10px;" onclick="saveGardenThresholds()">💾 บันทึกเกณฑ์ระดับน้ำสำหรับระบบรดน้ำ</button>
      </div>

      <div class="card" style="grid-column: span 2;">
        <div class="card-title">
          <span>ตารางเวลารดน้ำต้นไม้อัจฉริยะ (Smart Schedule)</span>
          <span id="gardenNtpStatus" style="font-size: 0.8rem; color: #10b981;">● NTP Synced</span>
        </div>
        <div style="font-size: 0.85rem; color: var(--text-muted); margin-bottom: 16px;">
          เพิ่ม/ลด ตารางเวลาได้อิสระ เลือกระบุเวลาเริ่ม - เวลาจบ เหมือนระบบ Smart Home โดย Master จะตัดวาล์ว Main A, เปิด Main B และเปิดเฉพาะโซนที่ตั้งไว้ตามเวลา
        </div>

        <div id="scheduleContainer">
          <!-- Dynamic Schedule Slots Generated Here -->
        </div>

        <div style="display: flex; flex-direction: column; gap: 12px; margin-top: 16px;">
          <button class="btn-add-sched" onclick="addScheduleSlot()">
            <span style="font-size: 1.2rem;">＋</span> เพิ่มช่วงเวลารดน้ำ
          </button>
          <button class="btn-save-sched" onclick="saveGardenSchedules()">
            <span>💾</span> บันทึกตารางเวลาทั้งหมด
          </button>
        </div>
      </div>
    </div>
  </div>

  <!-- ================= TAB: POOLS (สระว่ายน้ำ & เติมอัตโนมัติ) ================= -->
  <div id="tab-pool" class="tab-pane">
    <div class="grid">
      <!-- Wave Pool Auto Top-up Card -->
      <div class="card">
        <div class="card-title">
          <span>🌊 สระคลื่น (Wave Pool - Node 1)</span>
          <span id="autoPoolWaveBadge" style="font-size: 0.8rem; font-weight: 600; color: #10b981;">เติมอัตโนมัติ (ON)</span>
        </div>
        <div style="font-size: 0.8rem; color: var(--text-muted); margin-bottom: 12px;">
          เมื่อลูกลอยสระคลื่นลดระดับลง ระบบจะหน่วงเวลาตามที่ตั้งไว้ก่อนเริ่มเติมน้ำ (Priority ต่ำ: หากมีงานรดน้ำต้นไม้จะหยุดไว้ก่อน และกลับมาเติมต่อเมื่อว่าง)
        </div>
        <div class="input-row">
          <label>ระบบเติมน้ำอัตโนมัติ:</label>
          <select id="autoPoolWaveSelect">
            <option value="1">🟢 เปิดเติมอัตโนมัติเมื่อน้ำลด (Auto Top-Up ON)</option>
            <option value="0">⚪ ปิดระบบอัตโนมัติ - สั่งงานแมนนวลอย่างเดียว (OFF)</option>
          </select>
        </div>
        <div class="input-row">
          <label>รูปแบบการเติมน้ำ:</label>
          <select id="poolModeWave">
            <option value="0">⏱️ เติมตามระยะเวลาต่อรอบ (หากเต็มก่อนตัดทันที)</option>
            <option value="1">🌊 เติมจนกว่าจะถึงระดับเต็ม (เติมต่อเนื่องจนเต็ม)</option>
          </select>
        </div>
        <div class="input-row">
          <label>หน่วงเวลาก่อนเริ่มเติมน้ำ (นาที):</label>
          <input type="number" id="poolWaveDelay" min="0" max="180" placeholder="เช่น 5 (นาที)">
        </div>
        <div class="input-row">
          <label>ระยะเวลาเติมน้ำต่อรอบ (นาที):</label>
          <input type="number" id="poolWaveDur" min="1" max="180" placeholder="เช่น 15 (นาที)">
        </div>

        <div style="margin-top: 12px; padding-top: 10px; border-top: 1px dashed rgba(255,255,255,0.15);">
          <div style="font-size: 0.82rem; font-weight: 700; color: var(--accent-cyan); margin-bottom: 6px;">💧 เกณฑ์ระดับน้ำในแทงค์ 6,000L สำหรับสระคลื่น:</div>
          <div class="input-row">
            <label>ระดับน้ำขั้นต่ำที่จะเริ่มเติม (%):</label>
            <input type="number" id="pWStart" min="5" max="100" placeholder="เช่น 40 (%)">
          </div>
          <div class="input-row">
            <label>ระดับน้ำที่จะตัดหยุดพัก (%):</label>
            <input type="number" id="pWStop" min="5" max="95" placeholder="เช่น 20 (%)">
          </div>
        </div>

        <div id="waveQueueStatus" style="font-size: 0.8rem; color: var(--accent-yellow); margin-top: 6px; display: none;">
          ⏳ มีคิวรอน้ำเต็มสระคลื่น (กำลังรอระบบว่าง / พักชั่วคราว)
        </div>
      </div>

      <!-- Play Pool Auto Top-up Card -->
      <div class="card">
        <div class="card-title">
          <span>🏊 สระเล่น (Play Pool - Node 2)</span>
          <span id="autoPoolPlayBadge" style="font-size: 0.8rem; font-weight: 600; color: #10b981;">เติมอัตโนมัติ (ON)</span>
        </div>
        <div style="font-size: 0.8rem; color: var(--text-muted); margin-bottom: 12px;">
          เมื่อลูกลอยสระเล่นลดระดับลง ระบบจะหน่วงเวลาตามที่ตั้งไว้ก่อนเริ่มเติมน้ำ (Priority ต่ำ: หากมีงานรดน้ำต้นไม้จะหยุดไว้ก่อน และกลับมาเติมต่อเมื่อว่าง)
        </div>
        <div class="input-row">
          <label>ระบบเติมน้ำอัตโนมัติ:</label>
          <select id="autoPoolPlaySelect">
            <option value="1">🟢 เปิดเติมอัตโนมัติเมื่อน้ำลด (Auto Top-Up ON)</option>
            <option value="0">⚪ ปิดระบบอัตโนมัติ - สั่งงานแมนนวลอย่างเดียว (OFF)</option>
          </select>
        </div>
        <div class="input-row">
          <label>รูปแบบการเติมน้ำ:</label>
          <select id="poolModePlay">
            <option value="0">⏱️ เติมตามระยะเวลาต่อรอบ (หากเต็มก่อนตัดทันที)</option>
            <option value="1">🏊 เติมจนกว่าจะถึงระดับเต็ม (เติมต่อเนื่องจนเต็ม)</option>
          </select>
        </div>
        <div class="input-row">
          <label>หน่วงเวลาก่อนเริ่มเติมน้ำ (นาที):</label>
          <input type="number" id="poolPlayDelay" min="0" max="180" placeholder="เช่น 5 (นาที)">
        </div>
        <div class="input-row">
          <label>ระยะเวลาเติมน้ำต่อรอบ (นาที):</label>
          <input type="number" id="poolPlayDur" min="1" max="180" placeholder="เช่น 15 (นาที)">
        </div>

        <div style="margin-top: 12px; padding-top: 10px; border-top: 1px dashed rgba(255,255,255,0.15);">
          <div style="font-size: 0.82rem; font-weight: 700; color: #38bdf8; margin-bottom: 6px;">💧 เกณฑ์ระดับน้ำในแทงค์ 6,000L สำหรับสระเล่น:</div>
          <div class="input-row">
            <label>ระดับน้ำขั้นต่ำที่จะเริ่มเติม (%):</label>
            <input type="number" id="pPStart" min="5" max="100" placeholder="เช่น 40 (%)">
          </div>
          <div class="input-row">
            <label>ระดับน้ำที่จะตัดหยุดพัก (%):</label>
            <input type="number" id="pPStop" min="5" max="95" placeholder="เช่น 20 (%)">
          </div>
        </div>

        <div id="playQueueStatus" style="font-size: 0.8rem; color: var(--accent-yellow); margin-top: 6px; display: none;">
          ⏳ มีคิวรอน้ำเต็มสระเล่น (กำลังรอระบบว่าง / พักชั่วคราว)
        </div>
      </div>

      <!-- Pool Settings Save Button -->
      <div class="card" style="grid-column: span 2;">
        <button class="btn primary" style="width: 100%; font-size: 1rem; padding: 12px;" onclick="savePoolSettings()">💾 บันทึกการตั้งค่าสระว่ายน้ำและเกณฑ์ระดับน้ำ</button>
      </div>
    </div>
  </div>

  <!-- ================= TAB 4: TANK & BOREHOLE ================= -->
  <div id="tab-tank" class="tab-pane">
    <div class="grid">
      <!-- Auto Borehole Refill -->
      <div class="card">
        <div class="card-title">
          <span>ระบบเติมน้ำบาดาลอัตโนมัติ (Auto Borehole)</span>
          <span id="autoBoreholeBadge" style="font-size: 0.8rem; font-weight: 600; color: #10b981;">เปิดใช้งาน (ON)</span>
        </div>
        <div style="font-size: 0.8rem; color: var(--text-muted); margin-bottom: 12px;">
          สั่งเปิดปั๊มบาดาลเติมน้ำเข้าแทงค์อัตโนมัติเมื่อระดับน้ำลดลงต่ำกว่ากำหนด และปิดเมื่อน้ำเต็ม
        </div>
        <div class="input-row">
          <label>สถานะระบบอัตโนมัติ:</label>
          <select id="autoBoreholeSelect">
            <option value="1">🟢 เปิดระบบเติมน้ำอัตโนมัติ (Auto Refill ON)</option>
            <option value="0">⚪ ปิดระบบอัตโนมัติ - สั่งงานแมนนวลอย่างเดียว (OFF)</option>
          </select>
        </div>
        <div class="input-row">
          <label>สั่งเปิดปั๊มบาดาลเมื่อน้ำต่ำกว่า (%):</label>
          <input type="number" id="tankLowTrigger" placeholder="เช่น 70 (%)" min="10" max="90">
        </div>
        <div class="input-row">
          <label>สั่งปิดปั๊มบาดาลเมื่อน้ำเต็มถึง (%):</label>
          <input type="number" id="tankFullStop" placeholder="เช่น 95 (%)" min="50" max="100">
        </div>
        <button class="btn primary" style="width: 100%; margin-top: 8px;" onclick="saveBoreholeConfig()">บันทึกการตั้งค่าระบบบาดาล</button>
      </div>

      <!-- Node 3 Tank Calibration -->
      <div class="card">
        <div class="card-title">
          <span>ตั้งค่าระดับแทงค์น้ำ (Node 3 Calibration)</span>
          <span id="tankCalibStatus" style="font-size: 0.8rem; color: #10b981;">NVS Sync</span>
        </div>
        <div style="font-size: 0.8rem; color: var(--text-muted); margin-bottom: 12px;">
          กำหนดระยะจากหัวเซ็นเซอร์ Ultrasonic ถึงระดับน้ำ เพื่อให้คำนวณ % ปริมาตรน้ำได้ตรงกับแทงค์จริง
        </div>
        <div class="input-row">
          <label>ระยะเมื่อแทงค์น้ำแห้ง (0% Empty) [cm]:</label>
          <input type="number" id="tankEmptyCm" placeholder="เช่น 180 (cm)">
        </div>
        <div class="input-row">
          <label>ระยะเมื่อแทงค์น้ำเต็ม (100% Full) [cm]:</label>
          <input type="number" id="tankFullCm" placeholder="เช่น 25 (cm)">
        </div>
        <button class="btn primary" style="width: 100%; margin-top: 8px;" onclick="saveTankCalib()">บันทึกระดับแทงค์น้ำ (ส่งไป Node 3)</button>
      </div>

      <!-- ⚡ 2-Stage Adaptive Tank Sampling Card -->
      <div class="card" style="grid-column: span 2; border: 1.5px solid rgba(56, 189, 248, 0.4); background: linear-gradient(135deg, rgba(15, 23, 42, 0.85), rgba(14, 116, 144, 0.2));">
        <div class="card-title">
          <span>⚡ ความถี่การส่งข้อมูลเซ็นเซอร์แทงค์น้ำ (2-Stage Adaptive Sampling)</span>
          <span style="font-size: 0.8rem; color: #38bdf8; background: rgba(56, 189, 248, 0.15); padding: 3px 10px; border-radius: 12px;">Deep Sleep Sync</span>
        </div>
        <div style="font-size: 0.82rem; color: var(--text-muted); margin-bottom: 12px;">
          ระบบจะปรับความถี่การตื่นของ Node 3 อัตโนมัติ: สภาวะปกติจะส่งข้อมูลห่างเพื่อประหยัดแบตเตอรี่สูงสุด และจะสลับเป็นโหมดส่งถี่ทันทีเมื่อระดับน้ำใกล้เต็ม หรือขณะที่ปั๊มบาดาลกำลังสูบน้ำเข้าแทงค์
        </div>
        <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 16px;">
          <div style="background: rgba(15, 23, 42, 0.6); padding: 12px; border-radius: 10px; border: 1px solid rgba(255,255,255,0.08);">
            <div style="font-weight: 700; color: #38bdf8; margin-bottom: 8px;">🟢 ระดับปกติ (Normal Sampling)</div>
            <div class="input-row">
              <label>ความถี่ส่งข้อมูลช่วงปกติ (วินาที):</label>
              <input type="number" id="tankNormalIntervalSec" min="1" max="3600" placeholder="เช่น 30 (วินาที)">
            </div>
            <div style="font-size: 0.75rem; color: var(--text-muted);">ใช้เมื่อระดับน้ำต่ำกว่าเกณฑ์ และปั๊มบาดาลปิดอยู่</div>
          </div>
          <div style="background: rgba(15, 23, 42, 0.6); padding: 12px; border-radius: 10px; border: 1px solid rgba(255,255,255,0.08);">
            <div style="font-weight: 700; color: #f59e0b; margin-bottom: 8px;">⚡ ระดับใกล้เต็ม / กำลังเติม (Fast Sampling)</div>
            <div class="input-row">
              <label>ความถี่ส่งข้อมูลช่วงใกล้เต็ม (วินาที):</label>
              <input type="number" id="tankFastIntervalSec" min="1" max="60" placeholder="เช่น 3 (วินาที)">
            </div>
            <div class="input-row">
              <label>เริ่มส่งถี่เมื่อระดับน้ำถึง (%):</label>
              <input type="number" id="tankFastThresholdPct" min="10" max="99" placeholder="เช่น 80 (%)">
            </div>
          </div>
        </div>
        <button class="btn primary" style="width: 100%; margin-top: 12px;" onclick="saveTankSamplingConfig()">💾 บันทึกการตั้งค่าความถี่เซ็นเซอร์แทงค์น้ำ (Sync Node 3)</button>
      </div>
    </div>
  </div>

  <!-- ================= TAB 4: SYSTEM & WI-FI ================= -->
  <div id="tab-system" class="tab-pane">
    <div class="grid">
      <!-- 💤 Power Management & Sleep Schedule Card -->
      <div class="card" style="grid-column: span 2; border: 1.5px solid rgba(168, 85, 247, 0.4); background: linear-gradient(135deg, rgba(15, 23, 42, 0.9), rgba(88, 28, 135, 0.25));">
        <div class="card-title">
          <span style="color: #c084fc;">💤 ช่วงเวลาทำงาน & โหมดประหยัดพลังงาน (Power & Deep Sleep Schedule)</span>
          <span id="powerSleepBadge" style="font-size: 0.8rem; background: rgba(168, 85, 247, 0.2); color: #c084fc; padding: 3px 10px; border-radius: 12px;">Active Window</span>
        </div>
        <div style="font-size: 0.82rem; color: #cbd5e1; margin-bottom: 14px;">
          กำหนดช่วงเวลาทำงานของระบบ Master Controller และเซ็นเซอร์ Node 3 (เช่น 06:00 - 18:00) เมื่อพ้นเวลาทำงานระบบจะปิดโหลดทั้งหมดและเข้าสู่โหมด <strong>Deep Sleep</strong> จนถึงเวลาเริ่มทำงานของวันถัดไป เพื่อประหยัดพลังงานแบตเตอรี่และโซลาร์เซลล์
        </div>
        <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(260px, 1fr)); gap: 16px;">
          <div style="background: rgba(15, 23, 42, 0.6); padding: 14px; border-radius: 10px; border: 1px solid rgba(168, 85, 247, 0.3);">
            <div style="font-weight: 700; color: #c084fc; margin-bottom: 8px;">⚙️ โหมด Deep Sleep นอกเวลาทำงาน</div>
            <div class="input-row">
              <label>Master Deep Sleep นอกเวลา:</label>
              <select id="masterSleepSelect">
                <option value="1">🟢 เปิดใช้งาน (เข้า Deep Sleep หลังหมดเวลาทำงาน)</option>
                <option value="0">⚪ ปิดใช้งาน (เปิดทำงานตลอด 24 ชม.)</option>
              </select>
            </div>
            <div style="font-size: 0.75rem; color: #94a3b8; margin-top: 4px;">* เมื่อเปิดใช้งาน Master จะตัดปั๊มทั้งหมดและหลับช่วงกลางคืนอัตโนมัติ</div>
          </div>
          <div style="background: rgba(15, 23, 42, 0.6); padding: 14px; border-radius: 10px; border: 1px solid rgba(168, 85, 247, 0.3);">
            <div style="font-weight: 700; color: #38bdf8; margin-bottom: 8px;">🕒 กำหนดช่วงเวลาทำงาน (24 ชม.)</div>
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 12px;">
              <div>
                <label style="font-size: 0.8rem; color: var(--text-muted);">เริ่มทำงาน (ตื่น):</label>
                <div style="display: flex; gap: 4px; align-items: center; margin-top: 4px;">
                  <select id="actStartH" style="flex: 1; padding: 6px;"></select>
                  <span>:</span>
                  <select id="actStartM" style="flex: 1; padding: 6px;"></select>
                </div>
              </div>
              <div>
                <label style="font-size: 0.8rem; color: var(--text-muted);">สิ้นสุดทำงาน (หลับ):</label>
                <div style="display: flex; gap: 4px; align-items: center; margin-top: 4px;">
                  <select id="actEndH" style="flex: 1; padding: 6px;"></select>
                  <span>:</span>
                  <select id="actEndM" style="flex: 1; padding: 6px;"></select>
                </div>
              </div>
            </div>
          </div>
        </div>
        <button class="btn primary" style="width: 100%; margin-top: 12px; background: #9333ea; border-color: #a855f7;" onclick="savePowerConfig()">💾 บันทึกช่วงเวลาทำงานและโหมด Deep Sleep</button>
      </div>

      <!-- Solar Telemetry -->
      <div class="card">
        <div class="card-title">ข้อมูลแผงโซล่าเซลล์ & เซ็นเซอร์</div>
        <div class="pill-grid">
          <div class="pill">
            <div class="pill-label">แรงดันแผงโซล่าเซลล์</div>
            <div id="solarV" class="pill-val">-- V</div>
          </div>
          <div class="pill">
            <div class="pill-label">กำลังวัตต์แผง</div>
            <div id="solarW" class="pill-val">-- W</div>
          </div>
          <div class="pill">
            <div class="pill-label">ความสว่างแสงแดด</div>
            <div id="solarLux" class="pill-val">-- Lux</div>
          </div>
          <div class="pill">
            <div class="pill-label">Node 3 แบตเตอรี่</div>
            <div id="node3Bat" class="pill-val">-- V</div>
          </div>
        </div>
        <div style="font-size: 0.75rem; color: var(--text-muted); margin-top: 10px;" id="node4LastTime">
          🕒 Solar รับข้อมูลล่าสุด: --
        </div>
      </div>

      <!-- System Parameters Card (Offline Timeout) -->
      <div class="card">
        <div class="card-title">
          <span>ตั้งค่าตรวจจับเซ็นเซอร์ (Sensor Offline Timeout)</span>
          <span id="nodeOffStatusBadge" style="font-size: 0.8rem; color: #10b981;">NVS Sync</span>
        </div>
        <div style="font-size: 0.8rem; color: var(--text-muted); margin-bottom: 12px;">
          กำหนดระยะเวลาที่ไม่ได้รับข้อมูลจากเซ็นเซอร์ (Node 1, 2, 3, 4) ถึงจะระบุสถานะเป็น Offline (ออฟไลน์)
        </div>
        <div class="input-row">
          <label>ตรวจจับออฟไลน์เมื่อไม่ส่งข้อมูลเกิน (นาที):</label>
          <input type="number" id="nodeOfflineTimeoutMin" min="1" max="60" placeholder="เช่น 2 หรือ 3 (นาที)">
        </div>
        <button class="btn primary" style="width: 100%; margin-top: 8px;" onclick="saveSystemConfig()">💾 บันทึกเวลาตรวจจับออฟไลน์</button>
      </div>

      <!-- Simulator / Test Bench Card: จำลองระดับน้ำในแทงค์ และ สระว่ายน้ำเพื่อทดสอบระบบ -->
      <div class="card" style="border: 1.5px solid #a855f7; background: linear-gradient(135deg, rgba(30, 27, 75, 0.7), rgba(15, 23, 42, 0.8));">
        <div class="card-title">
          <span style="color: #c084fc;">🧪 เครื่องมือจำลองระดับน้ำ & เซ็นเซอร์ (Test Bench Simulator)</span>
          <span style="font-size: 0.8rem; background: rgba(168, 85, 247, 0.2); color: #c084fc; padding: 4px 10px; border-radius: 12px; border: 1px solid rgba(168, 85, 247, 0.4);">Test Bench</span>
        </div>
        <div style="font-size: 0.8rem; color: #cbd5e1; margin-bottom: 12px;">
          ใช้สำหรับทดสอบเงื่อนไขการทำงาน Safety Interlock และระบบเติมน้ำอัตโนมัติ โดยจำลองสถานะของ Node 1, Node 2, Node 3 ได้อิสระ
        </div>

        <!-- 1. Tank Level Simulator (Node 3) -->
        <div style="background: rgba(15, 23, 42, 0.6); padding: 12px; border-radius: 10px; border: 1px solid rgba(168, 85, 247, 0.3); margin-bottom: 14px;">
          <div style="font-size: 0.9rem; font-weight: 600; color: #38bdf8; margin-bottom: 8px;">💧 1. แทงค์น้ำ 6,000L (Node 3)</div>
          <div class="input-row" style="margin-bottom: 8px;">
            <label style="color: #e2e8f0;">ระดับน้ำจำลอง (%):</label>
            <div style="display: flex; gap: 8px; align-items: center;">
              <input type="range" id="simTankSlider" min="0" max="100" value="80" style="flex: 1; cursor: pointer;" oninput="document.getElementById('simTankVal').value = this.value">
              <input type="number" id="simTankVal" min="0" max="100" value="80" style="width: 75px; font-weight: 700; text-align: center;" oninput="document.getElementById('simTankSlider').value = this.value">
              <span style="font-weight: 600; color: #c084fc;">%</span>
            </div>
          </div>
          <div style="display: grid; grid-template-columns: repeat(4, 1fr); gap: 6px; margin-bottom: 10px;">
            <button class="btn" style="padding: 6px; font-size: 0.75rem; background: #7f1d1d;" onclick="setSimLevel(15)">💧 15% (วิกฤต)</button>
            <button class="btn" style="padding: 6px; font-size: 0.75rem; background: #854d0e;" onclick="setSimLevel(50)">💧 50% (บาดาล)</button>
            <button class="btn" style="padding: 6px; font-size: 0.75rem; background: #065f46;" onclick="setSimLevel(85)">💧 85% (พร้อมใช้)</button>
            <button class="btn" style="padding: 6px; font-size: 0.75rem; background: #1e3a8a;" onclick="setSimLevel(100)">💧 100% (เต็ม)</button>
          </div>
          <div style="display: flex; gap: 8px;">
            <button class="btn" style="flex: 1; padding: 8px; font-size: 0.85rem; background: #9333ea; font-weight: 600;" onclick="applySimTank(true)">⚡ จำลองระดับแทงค์นี้</button>
            <button class="btn" style="flex: 1; padding: 8px; font-size: 0.85rem; background: #334155;" onclick="applySimTank(false)">🔄 รีเซ็ตแทงค์จริง</button>
          </div>
        </div>

        <!-- 2. Pool 1 Wave Pool Simulator (Node 1) -->
        <div style="background: rgba(15, 23, 42, 0.6); padding: 12px; border-radius: 10px; border: 1px solid rgba(168, 85, 247, 0.3); margin-bottom: 14px;">
          <div style="font-size: 0.9rem; font-weight: 600; color: #06b6d4; margin-bottom: 8px;">🌊 2. สระคลื่น (Node 1)</div>
          <div style="display: flex; gap: 8px; flex-wrap: wrap;">
            <button class="btn" style="flex: 1; padding: 8px; font-size: 0.8rem; background: #b45309;" onclick="applySimPool('wave', true, true)">⚠️ จำลองน้ำลด (Low)</button>
            <button class="btn" style="flex: 1; padding: 8px; font-size: 0.8rem; background: #047857;" onclick="applySimPool('wave', true, false)">🟢 จำลองน้ำเต็ม (Full)</button>
            <button class="btn" style="padding: 8px 12px; font-size: 0.8rem; background: #334155;" onclick="applySimPool('wave', false, false)">🔄 รีเซ็ตจริง</button>
          </div>
        </div>

        <!-- 3. Pool 2 Play Pool Simulator (Node 2) -->
        <div style="background: rgba(15, 23, 42, 0.6); padding: 12px; border-radius: 10px; border: 1px solid rgba(168, 85, 247, 0.3);">
          <div style="font-size: 0.9rem; font-weight: 600; color: #3b82f6; margin-bottom: 8px;">🏊 3. สระเล่น (Node 2)</div>
          <div style="display: flex; gap: 8px; flex-wrap: wrap;">
            <button class="btn" style="flex: 1; padding: 8px; font-size: 0.8rem; background: #b45309;" onclick="applySimPool('play', true, true)">⚠️ จำลองน้ำลด (Low)</button>
            <button class="btn" style="flex: 1; padding: 8px; font-size: 0.8rem; background: #047857;" onclick="applySimPool('play', true, false)">🟢 จำลองน้ำเต็ม (Full)</button>
            <button class="btn" style="padding: 8px 12px; font-size: 0.8rem; background: #334155;" onclick="applySimPool('play', false, false)">🔄 รีเซ็ตจริง</button>
          </div>
        </div>
      </div>

      <!-- Wi-Fi Setup Card -->
      <div class="card">
        <div class="card-title">
          <span>ตั้งค่า Wi-Fi เชื่อมต่อระบบ</span>
          <span id="apTimer" style="font-size: 0.8rem; color: #f59e0b;">AP เปิด 1 นาที</span>
        </div>
        <div style="background: rgba(15, 23, 42, 0.5); padding: 12px; border-radius: 10px; margin-bottom: 12px; border: 1px solid var(--card-border);">
          <div style="display: flex; justify-content: space-between; align-items: center;">
            <span style="font-size: 0.85rem; color: var(--text-muted);">สถานะ Wi-Fi:</span>
            <span id="wifiStatusBadge" style="font-size: 0.85rem; font-weight: 600; color: #94a3b8;">กำลังตรวจสอบ...</span>
          </div>
          <div style="display: flex; justify-content: space-between; align-items: center; margin-top: 6px;">
            <span style="font-size: 0.85rem; color: var(--text-muted);">IP Address:</span>
            <span id="wifiIPText" style="font-size: 0.95rem; font-weight: 700; color: var(--accent-cyan);">--</span>
          </div>
        </div>
        <div class="input-row">
          <label>ชื่อ Wi-Fi (SSID):</label>
          <input type="text" id="wifiSSID" placeholder="ชื่อ Wi-Fi บ้าน / หน้างาน">
        </div>
        <div class="input-row">
          <label>รหัสผ่าน Wi-Fi (Password):</label>
          <input type="password" id="wifiPass" placeholder="รหัสผ่าน">
        </div>
        <button class="btn primary" style="width: 100%; margin-top: 8px;" onclick="saveWifi()">บันทึก Wi-Fi และเชื่อมต่อ</button>
      </div>

      <!-- OTA Firmware Update Card (GitHub Cloud + Local File) -->
      <div class="card" style="border: 1px solid rgba(56, 189, 248, 0.4); background: linear-gradient(145deg, rgba(15, 23, 42, 0.9), rgba(30, 41, 59, 0.6));">
        <div class="card-title">
          <span>🚀 อัปเดตเฟิร์มแวร์ (GitHub Cloud & Web OTA)</span>
          <span style="font-size: 0.8rem; background: rgba(56, 189, 248, 0.15); color: #38bdf8; padding: 2px 8px; border-radius: 6px; border: 1px solid rgba(56, 189, 248, 0.3);">Dual Slot (1.9MB)</span>
        </div>

        <div style="background: rgba(15, 23, 42, 0.6); padding: 12px; border-radius: 10px; margin-bottom: 14px; border: 1px solid rgba(255, 255, 255, 0.08);">
          <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px;">
            <span style="font-size: 0.85rem; color: var(--text-muted);">เวอร์ชันปัจจุบัน (Current Version):</span>
            <span id="currentVerBadge" style="font-size: 0.85rem; font-weight: 700; color: #10b981;">v1.3.0 (Dual-Core Ready)</span>
          </div>
          <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px;">
            <span style="font-size: 0.85rem; color: var(--text-muted);">การใช้หน่วยความจำ Flash:</span>
            <span style="font-size: 0.85rem; font-weight: 700; color: #38bdf8;">~65.8% (มีพื้นที่ว่าง ~670 KB)</span>
          </div>
          <div style="display: flex; justify-content: space-between; align-items: center;">
            <span style="font-size: 0.85rem; color: var(--text-muted);">ความปลอดภัย:</span>
            <span style="font-size: 0.85rem; color: #f59e0b;">🛡️ ตัดปั๊มทั้งหมดอัตโนมัติก่อน Flash</span>
          </div>
        </div>

        <!-- 1. GitHub Cloud Auto-Update Section -->
        <div style="background: rgba(15, 23, 42, 0.7); padding: 14px; border-radius: 10px; border: 1px solid rgba(56, 189, 248, 0.25); margin-bottom: 16px;">
          <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 10px;">
            <span style="font-size: 0.95rem; font-weight: 700; color: #38bdf8;">🌐 อัปเดตอัตโนมัติจาก GitHub Cloud</span>
            <span id="githubOnlineBadge" style="font-size: 0.75rem; color: #94a3b8;">พร้อมเชื่อมต่อ</span>
          </div>

          <div class="input-row" style="margin-bottom: 10px;">
            <label>GitHub Repository:</label>
            <div style="display: flex; gap: 8px;">
              <input type="text" id="githubRepoInput" value="penpencool/smartwater-master" placeholder="username/repository" style="flex: 1; font-family: monospace;">
              <button class="btn" style="background: #334155; padding: 6px 14px; font-size: 0.85rem; white-space: nowrap;" onclick="saveGithubRepo()">💾 บันทึก</button>
            </div>
          </div>

          <div style="display: flex; gap: 8px; margin-bottom: 10px;">
            <button id="ghCheckBtn" class="btn" style="flex: 1; background: #0284c7; padding: 10px; font-size: 0.9rem; font-weight: 600;" onclick="checkGithubUpdate()">
              🔍 ตรวจสอบเวอร์ชันล่าสุดบน GitHub
            </button>
          </div>

          <!-- GitHub Release Detail Box (ซ่อนไว้ก่อนตรวจเจอ) -->
          <div id="ghReleaseBox" style="display: none; background: rgba(2, 132, 199, 0.1); padding: 12px; border-radius: 8px; border: 1px solid rgba(56, 189, 248, 0.4); margin-bottom: 10px;">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px;">
              <span style="font-size: 0.85rem; color: var(--text-muted);">เวอร์ชันล่าสุดบน GitHub:</span>
              <strong id="ghLatestTag" style="font-size: 0.95rem; color: #34d399; font-family: monospace;">--</strong>
            </div>
            <div style="font-size: 0.85rem; color: white; font-weight: 600; margin-bottom: 4px;" id="ghReleaseName">--</div>
            <div style="font-size: 0.78rem; color: #94a3b8; line-height: 1.4; margin-bottom: 10px; white-space: pre-wrap;" id="ghReleaseBody">--</div>
            
            <button id="ghUpdateBtn" class="btn" style="width: 100%; background: linear-gradient(135deg, #10b981, #059669); color: white; font-weight: 700; padding: 12px; font-size: 0.95rem; border-radius: 8px; display: flex; align-items: center; justify-content: center; gap: 8px;" onclick="startGithubOTA()">
              <span>🚀</span>
              <span>ดาวน์โหลดและแฟลชจาก GitHub ทันที (Auto Flash)</span>
            </button>
          </div>
        </div>

        <!-- 2. Local File Upload Section -->
        <div style="background: rgba(15, 23, 42, 0.5); padding: 14px; border-radius: 10px; border: 1px solid rgba(255, 255, 255, 0.08);">
          <div style="font-size: 0.9rem; font-weight: 600; color: #cbd5e1; margin-bottom: 8px;">📁 หรืออัปโหลดไฟล์เฟิร์มแวร์จากเครื่อง (.bin)</div>
          <div class="input-row" style="margin-bottom: 8px;">
            <input type="file" id="otaFileInput" accept=".bin" style="width: 100%; padding: 8px; background: rgba(15, 23, 42, 0.7); border: 1px solid var(--card-border); border-radius: 8px; color: var(--text-main); font-size: 0.85rem;">
          </div>
          <button id="otaSubmitBtn" class="btn" style="width: 100%; background: #334155; color: white; font-weight: 600; font-size: 0.85rem; padding: 10px; border-radius: 8px;" onclick="startOTAUpdate()">
            ⚡ เริ่มอัปโหลดไฟล์จากเครื่อง (Start Local Flash)
          </button>
        </div>

        <!-- Common OTA Progress Container -->
        <div id="otaProgressBox" style="display: none; margin-top: 14px; background: rgba(15, 23, 42, 0.9); padding: 14px; border-radius: 10px; border: 1px solid rgba(56, 189, 248, 0.4);">
          <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px;">
            <span id="otaStatusText" style="font-size: 0.85rem; color: #38bdf8; font-weight: 600;">กำลังดำเนินการ...</span>
            <span id="otaPercentText" style="font-size: 0.95rem; font-weight: 700; color: #38bdf8; font-family: monospace;">0%</span>
          </div>
          <div style="width: 100%; height: 10px; background: rgba(255, 255, 255, 0.1); border-radius: 5px; overflow: hidden;">
            <div id="otaProgressBar" style="width: 0%; height: 100%; background: linear-gradient(90deg, #0284c7, #38bdf8); transition: width 0.2s;"></div>
          </div>
        </div>
      </div>
    </div>
  </div>
)rawliteral";

#endif // WEB_CONTENT_HTML_H
