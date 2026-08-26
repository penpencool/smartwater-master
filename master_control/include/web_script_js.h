#ifndef WEB_SCRIPT_JS_H
#define WEB_SCRIPT_JS_H

#include <Arduino.h>

const char DASHBOARD_JS[] PROGMEM = R"rawliteral(
let firstLoad = false;
let scheduleList = [];

function switchTab(tabId) {
  document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.getElementById(tabId).classList.add('active');
  if (window.event && window.event.currentTarget) {
    window.event.currentTarget.classList.add('active');
  }
}

function renderSchedules() {
  const container = document.getElementById('scheduleContainer');
  container.innerHTML = '';
  if (scheduleList.length === 0) {
    container.innerHTML = '<div style="text-align:center; padding: 24px; color: var(--text-muted);">ไม่มีตารางเวลาที่ตั้งไว้ กดปุ่ม "＋ เพิ่มช่วงเวลารดน้ำ" ด้านล่าง</div>';
    return;
  }

  scheduleList.forEach((s, idx) => {
    let startTot = s.startHour * 60 + s.startMin;
    let endTot = s.endHour * 60 + s.endMin;
    let dur = endTot - startTot;
    if (dur <= 0) dur += 24 * 60;

    // Generate 24-Hour Options (00 - 23)
    let startHourOptions = '';
    let endHourOptions = '';
    for (let h = 0; h < 24; h++) {
      const hStr = String(h).padStart(2, '0');
      startHourOptions += `<option value="${h}" ${s.startHour === h ? 'selected' : ''}>${hStr}</option>`;
      endHourOptions += `<option value="${h}" ${s.endHour === h ? 'selected' : ''}>${hStr}</option>`;
    }

    // Generate Minute Options (00, 05, 10, ... 55)
    let startMinOptions = '';
    let endMinOptions = '';
    for (let m = 0; m < 60; m += 5) {
      const mStr = String(m).padStart(2, '0');
      startMinOptions += `<option value="${m}" ${s.startMin === m ? 'selected' : ''}>${mStr}</option>`;
      endMinOptions += `<option value="${m}" ${s.endMin === m ? 'selected' : ''}>${mStr}</option>`;
    }

    const card = document.createElement('div');
    card.className = 'sched-card' + (s.enabled ? ' active-slot' : '');
    card.innerHTML = `
      <div class="sched-header">
        <div style="display: flex; align-items: center; gap: 14px;">
          <label class="switch">
            <input type="checkbox" ${s.enabled ? 'checked' : ''} onchange="scheduleList[${idx}].enabled = this.checked; renderSchedules();">
            <span class="slider"></span>
          </label>
          <strong style="color: ${s.enabled ? '#34d399' : '#94a3b8'}; font-size: 1.05rem; font-weight: 700;">
            ช่วงที่ ${idx + 1}: โซน ${s.zone} (${s.zone === 1 ? 'SV3' : 'SV4'})
          </strong>
        </div>
        <button style="background: rgba(239, 68, 68, 0.15); border: 1px solid rgba(239, 68, 68, 0.4); color: #f87171; font-size: 0.85rem; font-weight: 600; padding: 6px 12px; border-radius: 8px; cursor: pointer; display: flex; align-items: center; gap: 6px; transition: all 0.2s;" 
                onmouseover="this.style.background='#ef4444'; this.style.color='#ffffff';" 
                onmouseout="this.style.background='rgba(239, 68, 68, 0.15)'; this.style.color='#f87171';"
                onclick="removeScheduleSlot(${idx})">
          🗑️ ลบ
        </button>
      </div>

      <div class="time-picker-row">
        <div>
          <div class="sched-field-label">🌿 โซนรดน้ำ</div>
          <div class="sched-input-box">
            <select style="width: 100%;" onchange="scheduleList[${idx}].zone = parseInt(this.value); renderSchedules();">
              <option value="1" ${s.zone === 1 ? 'selected' : ''}>🌿 โซน 1 (SV3)</option>
              <option value="2" ${s.zone === 2 ? 'selected' : ''}>🌿 โซน 2 (SV4)</option>
            </select>
          </div>
        </div>

        <div>
          <div class="sched-field-label">🕒 เวลาเริ่ม (24 ชม.)</div>
          <div class="time-select-group">
            <select onchange="scheduleList[${idx}].startHour = parseInt(this.value); renderSchedules();">
              ${startHourOptions}
            </select>
            <span class="time-select-sep">:</span>
            <select onchange="scheduleList[${idx}].startMin = parseInt(this.value); renderSchedules();">
              ${startMinOptions}
            </select>
          </div>
        </div>

        <div>
          <div class="sched-field-label">🕒 เวลาจบ (24 ชม.)</div>
          <div class="time-select-group">
            <select onchange="scheduleList[${idx}].endHour = parseInt(this.value); renderSchedules();">
              ${endHourOptions}
            </select>
            <span class="time-select-sep">:</span>
            <select onchange="scheduleList[${idx}].endMin = parseInt(this.value); renderSchedules();">
              ${endMinOptions}
            </select>
          </div>
        </div>

        <div>
          <div class="sched-field-label">&nbsp;</div>
          <div class="sched-dur-badge ${s.enabled ? '' : 'disabled'}">
            <span>💧</span>
            <span>รดน้ำ ${dur} นาที</span>
          </div>
        </div>
      </div>
    `;
    container.appendChild(card);
  });
}

function addScheduleSlot() {
  if (scheduleList.length >= 6) {
    showNotification('เพิ่มได้สูงสุด 6 ตารางเวลา', 'error');
    return;
  }
  scheduleList.push({
    enabled: true,
    zone: 1,
    startHour: 6,
    startMin: 0,
    endHour: 6,
    endMin: 15
  });
  renderSchedules();
}

function removeScheduleSlot(idx) {
  scheduleList.splice(idx, 1);
  renderSchedules();
}

function saveGardenSchedules() {
  fetch('/api/garden_config', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ schedules: scheduleList })
  })
  .then(res => res.json())
  .then(res => showNotification(res.msg || 'บันทึกตารางเวลาเรียบร้อย', 'success'))
  .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
}

function formatMinSec(totalSec) {
  if (totalSec <= 0) return '00:00';
  const m = Math.floor(totalSec / 60);
  const s = totalSec % 60;
  return String(m).padStart(2, '0') + ':' + String(s).padStart(2, '0');
}

function updateData() {
  fetch('/api/status')
    .then(res => res.json())
    .then(data => {
      if (!data) return;

      // Time & NTP
      const timeText = data.currentTime ? ('🕒 ' + data.currentTime) : '🕒 --:--:--';
      const timeBadgeEl = document.getElementById('timeBadge');
      if (timeBadgeEl) timeBadgeEl.innerText = timeText;

      const ntpBadge = document.getElementById('gardenNtpStatus');
      if (ntpBadge) {
        if (data.ntpSynced) {
          ntpBadge.innerText = '● NTP Synced (' + (data.currentTime || '') + ')';
          ntpBadge.style.color = '#10b981';
        } else {
          ntpBadge.innerText = '● ไม่สามารถซิงค์เวลาได้ (No NTP)';
          ntpBadge.style.color = '#ef4444';
        }
      }

      // Node 1 (Wave Pool)
      const node1Badge = document.getElementById('node1Badge');
      const node1Float = document.getElementById('node1FloatTxt');
      const node1Bat = document.getElementById('node1BatTxt');
      const node1Time = document.getElementById('node1LastTime');
      if (data.node1Online) {
        node1Badge.innerText = '● Online';
        node1Badge.style.color = '#10b981';
        node1Float.innerText = data.node1WaterLow ? '⚠️ น้ำลด (ต้องการเติม)' : '🟢 ปกติ (น้ำเต็ม)';
        node1Float.style.color = data.node1WaterLow ? '#ef4444' : '#10b981';
        node1Bat.innerText = data.node1Battery.toFixed(2) + ' V';
        if (node1Time) node1Time.innerText = '🕒 รับข้อมูลล่าสุด: ' + (data.lastNode1Time || '--:--:--');
      } else {
        node1Badge.innerText = '● Offline';
        node1Badge.style.color = '#ef4444';
        node1Float.innerText = 'ออฟไลน์';
        node1Float.style.color = '#94a3b8';
        node1Bat.innerText = '-- V';
        if (node1Time) {
          node1Time.innerText = '🕒 ขาดการติดต่อตั้งแต่: ' + (data.lastNode1Time || 'ไม่เคยเชื่อมต่อ');
        }
      }

      // Node 2 (Play Pool)
      const node2Badge = document.getElementById('node2Badge');
      const node2Float = document.getElementById('node2FloatTxt');
      const node2Bat = document.getElementById('node2BatTxt');
      const node2Time = document.getElementById('node2LastTime');
      if (data.node2Online) {
        node2Badge.innerText = '● Online';
        node2Badge.style.color = '#10b981';
        node2Float.innerText = data.node2WaterLow ? '⚠️ น้ำลด (ต้องการเติม)' : '🟢 ปกติ (น้ำเต็ม)';
        node2Float.style.color = data.node2WaterLow ? '#ef4444' : '#10b981';
        node2Bat.innerText = data.node2Battery.toFixed(2) + ' V';
        if (node2Time) node2Time.innerText = '🕒 รับข้อมูลล่าสุด: ' + (data.lastNode2Time || '--:--:--');
      } else {
        node2Badge.innerText = '● Offline';
        node2Badge.style.color = '#ef4444';
        node2Float.innerText = 'ออฟไลน์';
        node2Float.style.color = '#94a3b8';
        node2Bat.innerText = '-- V';
        if (node2Time) {
          node2Time.innerText = '🕒 ขาดการติดต่อตั้งแต่: ' + (data.lastNode2Time || 'ไม่เคยเชื่อมต่อ');
        }
      }

      // Node 3 Tank
      const node3StatusEl = document.getElementById('node3Status');
      const node3Time = document.getElementById('node3LastTime');
      if (data.node3Online) {
        node3StatusEl.innerText = '● Online';
        node3StatusEl.style.color = '#10b981';
        const lvl = data.tankLevel.toFixed(1);
        document.getElementById('tankLevelText').innerText = lvl + ' %';
        document.getElementById('tankLevelBar').style.height = lvl + '%';
        document.getElementById('tankDistText').innerText = 'ระยะวัด: ' + data.tankDist.toFixed(1) + ' cm';
        document.getElementById('tankFloatText').innerText = 'ลูกลอย Backup: ' + (data.tankFloat ? '⚠️ สูงสุด (ตัด)' : 'ปกติ');
        if (node3Time) node3Time.innerText = '🕒 รับข้อมูลล่าสุด: ' + (data.lastNode3Time || '--:--:--');
      } else {
        node3StatusEl.innerText = '● Offline';
        node3StatusEl.style.color = '#ef4444';
        document.getElementById('tankLevelText').innerText = '-- %';
        document.getElementById('tankLevelBar').style.height = '0%';
        document.getElementById('tankDistText').innerText = 'ระยะวัด: ออฟไลน์';
        document.getElementById('tankFloatText').innerText = 'ลูกลอย Backup: ไม่ทราบสถานะ';
        if (node3Time) {
          node3Time.innerText = '🕒 ขาดการติดต่อตั้งแต่: ' + (data.lastNode3Time || 'ไม่เคยเชื่อมต่อ');
        }
      }

      // Node 4 Solar
      const node4Time = document.getElementById('node4LastTime');
      if (node4Time) {
        if (data.node4Online) {
          node4Time.innerText = '🕒 Solar รับข้อมูลล่าสุด: ' + (data.lastNode4Time || '--:--:--');
        } else {
          node4Time.innerText = '🕒 Solar ขาดการติดต่อตั้งแต่: ' + (data.lastNode4Time || 'ไม่เคยเชื่อมต่อ');
        }
      }

      // Live Task Countdown Timer Box
      const countBox = document.getElementById('taskCountdownBox');
      const countLabel = document.getElementById('taskCountdownLabel');
      const countVal = document.getElementById('taskCountdownVal');
      if (data.taskRemainingSec > 0 && data.activeTask) {
        countBox.style.display = 'flex';
        countLabel.innerText = '🔵 ' + data.activeTask + ' (เหลือเวลา):';
        countVal.innerText = '⏱️ ' + formatMinSec(data.taskRemainingSec);
      } else {
        countBox.style.display = 'none';
      }

      // First Load Data Init
      if (!firstLoad) {
        if (data.tankEmptyCm > 0) {
          document.getElementById('tankEmptyCm').value = data.tankEmptyCm;
          document.getElementById('tankFullCm').value = data.tankFullCm;
          document.getElementById('tankLowTrigger').value = data.tankLowTrigger;
          document.getElementById('tankFullStop').value = data.tankFullStop;
          document.getElementById('autoBoreholeSelect').value = data.autoBorehole ? "1" : "0";
        }

        // Pool Top-up Settings
        document.getElementById('autoPoolWaveSelect').value = data.autoPoolWave ? "1" : "0";
        document.getElementById('poolModeWave').value = (data.poolModeWave !== undefined) ? String(data.poolModeWave) : "0";
        document.getElementById('poolWaveDelay').value = data.poolWaveDelay || 5;
        document.getElementById('poolWaveDur').value = data.poolWaveDur || 15;

        document.getElementById('autoPoolPlaySelect').value = data.autoPoolPlay ? "1" : "0";
        document.getElementById('poolModePlay').value = (data.poolModePlay !== undefined) ? String(data.poolModePlay) : "0";
        document.getElementById('poolPlayDelay').value = data.poolPlayDelay || 5;
        document.getElementById('poolPlayDur').value = data.poolPlayDur || 15;

        // System Parameters
        if (data.nodeOfflineTimeoutMin !== undefined) {
          document.getElementById('nodeOfflineTimeoutMin').value = data.nodeOfflineTimeoutMin;
        }

        if (data.wifiSSID && data.wifiSSID.length > 0) {
          document.getElementById('wifiSSID').value = data.wifiSSID;
        }

        if (data.githubRepo && data.githubRepo.length > 0) {
          const ghInput = document.getElementById('githubRepoInput');
          if (ghInput) ghInput.value = data.githubRepo;
        }

        if (data.firmwareVer) {
          const verBadge = document.getElementById('currentVerBadge');
          if (verBadge) verBadge.innerText = data.firmwareVer + ' (Dual OTA Ready)';
        }

        if (data.schedules) {
          scheduleList = data.schedules;
          renderSchedules();
        }
        firstLoad = true;
      }

      // Hero Status Card (Top of Tab 1 Overview)
      const heroText = document.getElementById('mainHeroStatusText');
      const heroSub = document.getElementById('mainHeroSubText');
      const heroTimerBox = document.getElementById('mainHeroTimerBadge');
      const heroTimerVal = document.getElementById('mainHeroTimerVal');

      if (heroText && heroSub && heroTimerBox && heroTimerVal) {
        if (data.errorCode > 0) {
          heroText.innerHTML = '<span style="color:#ef4444;">🔴 พบข้อผิดพลาด (Alarm Error E' + data.errorCode + ')</span>';
          heroSub.innerText = 'ระบบตัดการทำงานชั่วคราวเพื่อความปลอดภัย กรุณาตรวจสอบหรือกดปุ่มรีเซ็ต Alarm';
          heroTimerBox.style.display = 'none';
        } else if (data.activeTask && data.activeTask.length > 0) {
          heroText.innerHTML = '<span style="color:#38bdf8;">🔵 กำลังทำงาน: ' + data.activeTask + '</span>';
          heroSub.innerText = 'ระบบกำลังทำงานตามคำสั่งอย่างต่อเนื่อง และจะหยิบคิวงานถัดไปมาทำอัตโนมัติเมื่อเสร็จสิ้น';
          if (data.taskRemainingSec > 0) {
            heroTimerBox.style.display = 'block';
            heroTimerVal.innerText = '⏱️ ' + formatMinSec(data.taskRemainingSec);
          } else {
            heroTimerBox.style.display = 'none';
          }
        } else if (data.pumpBorehole) {
          heroText.innerHTML = '<span style="color:#10b981;">🚰 กำลังสูบน้ำบาดาลเติมเข้าแทงค์ (Borehole Refill)</span>';
          heroSub.innerText = 'ปั๊มบาดาล 1,500W กำลังทำงานเติมน้ำเข้าแทงค์ 6,000L จนถึงระดับเต็ม';
          heroTimerBox.style.display = 'none';
        } else {
          heroText.innerHTML = '<span style="color:#34d399;">🟢 ระบบว่าง (Standby / พร้อมทำงาน)</span>';
          heroSub.innerText = 'ไม่มีงานที่กำลังทำงานอยู่ พร้อมรับคำสั่งงานด่วน หรือรอรอบตารางเวลาอัตโนมัติ';
          heroTimerBox.style.display = 'none';
        }
      }

      // Live Waiting Queue List
      const qContainer = document.getElementById('heroQueueContainer');
      const qCountBadge = document.getElementById('heroQueueCountBadge');
      const qList = document.getElementById('heroQueueList');

      if (qContainer && qList && data.queue && data.queue.length > 0) {
        qContainer.style.display = 'block';
        qCountBadge.innerText = data.queue.length + ' งานรอคิว';
        qList.innerHTML = '';

        data.queue.forEach((item, qIdx) => {
          let pIcon = '🔹';
          let pColor = '#38bdf8';
          let originBadge = '';
          
          if (item.type === 1) { 
            pIcon = '⚡ P1 ด่วน'; 
            pColor = '#f59e0b';
            originBadge = '<span style="font-size:0.7rem; background:rgba(245,158,11,0.2); color:#f59e0b; padding:2px 6px; border-radius:4px; border:1px solid rgba(245,158,11,0.4);">👤 สั่งงานด่วน</span>';
          }
          else if (item.type === 2) { 
            pIcon = '⏰ P2 ตาราง'; 
            pColor = '#10b981';
            originBadge = '<span style="font-size:0.7rem; background:rgba(16,185,129,0.2); color:#10b981; padding:2px 6px; border-radius:4px; border:1px solid rgba(16,185,129,0.4);">🤖 ตารางเวลา</span>';
          }
          else if (item.type === 3) { 
            pIcon = '🏊 P3 สระด่วน'; 
            pColor = '#06b6d4';
            originBadge = '<span style="font-size:0.7rem; background:rgba(6,182,212,0.2); color:#06b6d4; padding:2px 6px; border-radius:4px; border:1px solid rgba(6,182,212,0.4);">👤 สั่งงานด่วน</span>';
          }
          else if (item.type === 4) { 
            pIcon = '💧 P4 ออโต้'; 
            pColor = '#94a3b8';
            originBadge = '<span style="font-size:0.7rem; background:rgba(148,163,184,0.2); color:#cbd5e1; padding:2px 6px; border-radius:4px; border:1px solid rgba(148,163,184,0.4);">🤖 อัตโนมัติ (ลูกลอย)</span>';
          }

          let delayInfo = '';
          if (item.delaySec > 0) {
            delayInfo = `<span style="font-size:0.75rem; color:#f59e0b; font-family:monospace; background:rgba(245,158,11,0.15); padding:2px 6px; border-radius:4px;">⏳ เริ่มในอีก ${formatMinSec(item.delaySec)}</span>`;
          } else {
            delayInfo = `<span style="font-size:0.75rem; color:#34d399; background:rgba(52,211,153,0.15); padding:2px 6px; border-radius:4px;">🟢 พร้อมทำทันที</span>`;
          }

          const qItemEl = document.createElement('div');
          qItemEl.style.cssText = 'display:flex; justify-content:space-between; align-items:center; background:rgba(15,23,42,0.7); padding:10px 14px; border-radius:10px; border:1px solid rgba(255,255,255,0.08); font-size:0.85rem; flex-wrap:wrap; gap:8px;';
          qItemEl.innerHTML = `
            <div style="display:flex; align-items:center; gap:8px; flex-wrap:wrap;">
              <span style="font-size:0.75rem; font-weight:700; background:rgba(255,255,255,0.1); color:${pColor}; padding:3px 8px; border-radius:6px;">${pIcon}</span>
              ${originBadge}
              <strong style="color:white; font-size:0.9rem;">${qIdx + 1}. ${item.name}</strong>
            </div>
            <div style="display:flex; align-items:center; gap:10px;">
              ${delayInfo}
              <span style="color:#38bdf8; font-family:monospace; font-weight:700;">⏱️ ${item.dur} นาที</span>
            </div>
          `;
          qList.appendChild(qItemEl);
        });
      } else if (qContainer) {
        qContainer.style.display = 'none';
      }

      // Pool Auto Top-up Badges
      const waveBadge = document.getElementById('autoPoolWaveBadge');
      if (waveBadge) {
        waveBadge.innerText = data.autoPoolWave ? 'เติมอัตโนมัติ (ON)' : 'ปิดใช้งาน (OFF)';
        waveBadge.style.color = data.autoPoolWave ? '#10b981' : '#94a3b8';
      }
      const playBadge = document.getElementById('autoPoolPlayBadge');
      if (playBadge) {
        playBadge.innerText = data.autoPoolPlay ? 'เติมอัตโนมัติ (ON)' : 'ปิดใช้งาน (OFF)';
        playBadge.style.color = data.autoPoolPlay ? '#10b981' : '#94a3b8';
      }

      // Auto Borehole Badge
      const autoBadge = document.getElementById('autoBoreholeBadge');
      if (data.autoBorehole) {
        autoBadge.innerText = 'เปิดใช้งาน (ON)';
        autoBadge.style.color = '#10b981';
      } else {
        autoBadge.innerText = 'ปิดใช้งาน (OFF)';
        autoBadge.style.color = '#94a3b8';
      }

      // Relays (Full 8 Relays)
      setRelayUI('r1_tag', 'r1_txt', data.pumpBorehole);
      setRelayUI('r2_tag', 'r2_txt', data.pumpFilter);
      setRelayUI('r4_tag', 'r4_txt', data.mainA);
      setRelayUI('r5_tag', 'r5_txt', data.mainB);
      setRelayUI('r6_tag', 'r6_txt', data.sv1);
      setRelayUI('r7_tag', 'r7_txt', data.sv2);
      setRelayUI('r7b_tag', 'r7b_txt', data.sv3);
      setRelayUI('r8_tag', 'r8_txt', data.sv4);

      // Sync Borehole Toggle Switch UI
      const bSwitch = document.getElementById('boreholeToggleSwitch');
      const bStatus = document.getElementById('boreholeToggleStatus');
      if (bSwitch && bStatus) {
        bSwitch.checked = data.pumpBorehole;
        if (data.pumpBorehole) {
          bStatus.innerText = '(🟢 เปิดทำงาน ON)';
          bStatus.style.color = '#10b981';
        } else {
          bStatus.innerText = '(⚪ ปิด OFF)';
          bStatus.style.color = '#94a3b8';
        }
      }

      // Alert Banner & Error Diagnostic
      const banner = document.getElementById('alertBanner');
      const alertMsg = document.getElementById('alertMsg');
      const sysBadge = document.getElementById('sysBadge');
      const resetBtn = document.getElementById('alertResetBtn');

      if (data.errorCode > 0) {
        banner.className = 'show-err';
        if (resetBtn) resetBtn.style.display = 'inline-block';
        sysBadge.className = 'status-badge error';
        let errMsg = 'พบข้อผิดพลาดในระบบ';
        if (data.errorCode === 1) errMsg = 'ปั๊มเดินแต่น้ำไม่ไหล (Flow Timeout)';
        else if (data.errorCode === 2) errMsg = 'น้ำในแทงค์ต่ำวิกฤต (<= 25%)';
        else if (data.errorCode === 3) errMsg = 'Node 3 แทงค์น้ำออฟไลน์ (ตัดปั๊มบาดาลเพื่อความปลอดภัย)';
        else if (data.errorCode === 4) errMsg = 'แสงแดดอ่อน / โวลต์โซล่าเซลล์ตก';
        else if (data.errorCode === 5) errMsg = 'ซิงค์เวลาจากอินเทอร์เน็ตไม่สำเร็จ (NTP Failed) - ตารางเวลาอาจไม่เริ่ม';
        else if (data.errorCode === 99) errMsg = 'หยุดฉุกเฉิน (EMERGENCY STOP)';
        
        alertMsg.innerText = '⚠️ ข้อผิดพลาด (E' + data.errorCode + '): ' + errMsg;
        sysBadge.innerText = '🔴 ERROR E' + data.errorCode;
      } else if (!data.node3Online) {
        banner.className = 'show-warn';
        if (resetBtn) resetBtn.style.display = 'none';
        alertMsg.innerText = '⚠️ คำเตือน: Node 3 เซ็นเซอร์แทงค์น้ำออฟไลน์ ขาดการติดต่อตั้งแต่ ' + (data.lastNode3Time || 'ไม่ทราบเวลา');
        sysBadge.className = 'status-badge';
        sysBadge.innerText = '🟡 Standby (Node 3 Offline)';
      } else {
        banner.className = '';
        if (resetBtn) resetBtn.style.display = 'none';
        sysBadge.className = 'status-badge';
        if (data.taskRemainingSec > 0 && data.activeTask) {
          sysBadge.innerText = '🔵 ' + data.activeTask + ' (' + formatMinSec(data.taskRemainingSec) + ')';
        } else {
          sysBadge.innerText = data.activeTask ? '🔵 กำลังทำงาน: ' + data.activeTask : '🟢 ระบบปกติ (Idle)';
        }
      }

      // Telemetry
      document.getElementById('node3Bat').innerText = data.node3Battery.toFixed(2) + ' V';
      document.getElementById('solarV').innerText = data.solarVolt.toFixed(1) + ' V';
      document.getElementById('solarW').innerText = data.solarWatt.toFixed(0) + ' W';
      document.getElementById('solarLux').innerText = data.solarLux.toFixed(0) + ' Lux';

      // Wi-Fi
      const wifiBadge = document.getElementById('wifiStatusBadge');
      const wifiIP = document.getElementById('wifiIPText');
      if (data.wifiConnected) {
        wifiBadge.innerText = '🟢 เชื่อมต่อแล้ว (' + (data.wifiSSID || 'Wi-Fi') + ')';
        wifiBadge.style.color = '#10b981';
        wifiIP.innerText = data.staIP;
      } else {
        wifiBadge.innerText = '⚪ กำลังเชื่อมต่อ / ออฟไลน์';
        wifiBadge.style.color = '#f59e0b';
        wifiIP.innerText = '192.168.4.1 (AP Mode)';
      }

      // AP Countdown
      const apTimerEl = document.getElementById('apTimer');
      if (data.apActive) {
        if (data.apRemainingSec < 0) {
          apTimerEl.innerText = '🟢 กำลังเชื่อมต่อ AP (' + data.apClients + ' เครื่อง)';
          apTimerEl.style.color = '#10b981';
        } else {
          apTimerEl.innerText = '⚡ AP ปิดใน ' + data.apRemainingSec + 's (หากไม่มีคนต่อ)';
          apTimerEl.style.color = '#f59e0b';
        }
      } else {
        apTimerEl.innerText = '🔒 AP ปิดแล้ว';
        apTimerEl.style.color = '#94a3b8';
      }
    })
    .catch(err => console.error(err));
}

// Toast Label Notification
let toastTimer = null;
function showNotification(msg, type = 'info') {
  const toast = document.getElementById('notifyToast');
  const icon = document.getElementById('notifyIcon');
  const text = document.getElementById('notifyText');

  if (!toast) return;

  toast.className = type;
  if (type === 'success') icon.innerText = '✅';
  else if (type === 'error') icon.innerText = '❌';
  else icon.innerText = 'ℹ️';

  text.innerText = msg;
  toast.classList.add('show');

  if (toastTimer) clearTimeout(toastTimer);
  toastTimer = setTimeout(() => {
    toast.classList.remove('show');
  }, 3500);
}

function setRelayUI(tagId, txtId, isActive) {
  const tag = document.getElementById(tagId);
  const txt = document.getElementById(txtId);
  if (!tag || !txt) return;
  if (isActive) {
    tag.className = 'relay-tag active';
    txt.innerText = 'ON / OPEN';
    txt.style.color = '#10b981';
  } else {
    tag.className = 'relay-tag';
    txt.innerText = 'OFF / CLOSED';
    txt.style.color = '#94a3b8';
  }
}

function toggleBoreholeSwitch(isChecked) {
  const action = isChecked ? 'pump_borehole_on' : 'pump_borehole_off';
  fetch('/api/command?action=' + action, { method: 'POST' })
    .then(res => res.json())
    .then(res => {
      showNotification(res.msg || (isChecked ? 'เปิดปั๊มบาดาล' : 'ปิดปั๊มบาดาล'), isChecked ? 'success' : 'info');
      updateData();
    })
    .catch(err => {
      showNotification('เกิดข้อผิดพลาด: ' + err, 'error');
      updateData();
    });
}

function sendCommand(cmd) {
  fetch('/api/command?action=' + cmd, { method: 'POST' })
    .then(res => res.json())
    .then(res => {
      showNotification(res.msg || 'ดำเนินการสำเร็จ', 'success');
      updateData();
    })
    .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
}

// Modal Duration Dialog
let currentModalAction = null;
function openDurModal(title, label, defaultVal, callback) {
  document.getElementById('durModalTitle').innerText = title;
  document.getElementById('durModalLabel').innerText = label;
  document.getElementById('durInput').value = defaultVal;
  currentModalAction = callback;
  document.getElementById('durModal').classList.add('active');
}

function closeDurModal() {
  document.getElementById('durModal').classList.remove('active');
  currentModalAction = null;
}

document.getElementById('durConfirmBtn').addEventListener('click', function() {
  const val = parseInt(document.getElementById('durInput').value);
  if (isNaN(val) || val <= 0) {
    showNotification('กรุณาระบุจำนวนนาทีที่ถูกต้อง', 'error');
    return;
  }
  if (currentModalAction) {
    currentModalAction(val);
  }
  closeDurModal();
});

function sendGardenCommand(zone) {
  openDurModal('🌱 ตั้งเวลารดน้ำ Zone ' + zone, 'ระบุระยะเวลารดน้ำ (นาที):', 15, function(dur) {
    fetch('/api/command?action=garden_zone' + zone + '_start&dur=' + dur, { method: 'POST' })
      .then(res => res.json())
      .then(res => {
        showNotification(res.msg || 'เริ่มรดน้ำ', 'success');
        updateData();
      })
      .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
  });
}

function sendPoolCommand(poolType) {
  const name = (poolType === 'wave') ? '🌊 เติมน้ำสระคลื่น (SV1)' : '🏊 เติมน้ำสระเล่น (SV2)';
  openDurModal(name, 'ระบุระยะเวลาเติมน้ำ (นาที):', 15, function(dur) {
    fetch('/api/command?action=pool_' + poolType + '_start&dur=' + dur, { method: 'POST' })
      .then(res => res.json())
      .then(res => {
        showNotification(res.msg || 'เริ่มเติมน้ำสระ', 'success');
        updateData();
      })
      .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
  });
}

function savePoolSettings() {
  const autoWave = document.getElementById('autoPoolWaveSelect').value;
  const poolModeWave = document.getElementById('poolModeWave').value;
  const waveDelay = parseInt(document.getElementById('poolWaveDelay').value);
  const waveDur = parseInt(document.getElementById('poolWaveDur').value);

  const autoPlay = document.getElementById('autoPoolPlaySelect').value;
  const poolModePlay = document.getElementById('poolModePlay').value;
  const playDelay = parseInt(document.getElementById('poolPlayDelay').value);
  const playDur = parseInt(document.getElementById('poolPlayDur').value);

  if (isNaN(waveDelay) || isNaN(waveDur) || isNaN(playDelay) || isNaN(playDur) || waveDur <= 0 || playDur <= 0) {
    showNotification('กรุณาระบุตัวเลขหน่วงเวลาและระยะเวลาเติมน้ำให้ถูกต้อง', 'error');
    return;
  }

  const url = `/api/pool_config?autoWave=${autoWave}&poolModeWave=${poolModeWave}&waveDelay=${waveDelay}&waveDur=${waveDur}&autoPlay=${autoPlay}&poolModePlay=${poolModePlay}&playDelay=${playDelay}&playDur=${playDur}`;
  fetch(url, { method: 'POST' })
    .then(res => res.json())
    .then(res => showNotification(res.msg || 'บันทึกการตั้งค่าสระว่ายน้ำสำเร็จ', 'success'))
    .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
}

function saveBoreholeConfig() {
  const autoVal = document.getElementById('autoBoreholeSelect').value;
  const lowVal = parseFloat(document.getElementById('tankLowTrigger').value);
  const fullVal = parseFloat(document.getElementById('tankFullStop').value);

  if (isNaN(lowVal) || isNaN(fullVal) || lowVal >= fullVal || lowVal < 5 || fullVal > 100) {
    showNotification('กรุณากรอกตัวเลขเปอร์เซ็นต์ที่ถูกต้อง (ระดับเปิดเติม ต้องน้อยกว่า ระดับตัด)', 'error');
    return;
  }

  fetch('/api/borehole_config?auto=' + autoVal + '&lowTrigger=' + lowVal + '&fullStop=' + fullVal, { method: 'POST' })
    .then(res => res.json())
    .then(res => showNotification(res.msg || 'บันทึกเรียบร้อย', 'success'))
    .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
}

function saveTankCalib() {
  const emptyCm = parseFloat(document.getElementById('tankEmptyCm').value);
  const fullCm = parseFloat(document.getElementById('tankFullCm').value);
  if (isNaN(emptyCm) || isNaN(fullCm) || emptyCm <= fullCm) {
    showNotification('กรุณากรอกตัวเลขระยะทางที่ถูกต้อง (ระยะแทงค์แห้ง ต้องมากกว่า ระยะแทงค์เต็ม)', 'error');
    return;
  }
  fetch('/api/tank_calibrate?empty=' + emptyCm + '&full=' + fullCm, { method: 'POST' })
    .then(res => res.json())
    .then(res => showNotification(res.msg || 'ส่งคำสั่งบันทึกไปยัง Node 3 สำเร็จ', 'success'))
    .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
}

function saveSystemConfig() {
  const nodeOffMin = parseInt(document.getElementById('nodeOfflineTimeoutMin').value);
  if (isNaN(nodeOffMin) || nodeOffMin < 1 || nodeOffMin > 60) {
    showNotification('กรุณาระบุระยะเวลา 1 - 60 นาที', 'error');
    return;
  }
  fetch('/api/system_config?nodeOffMin=' + nodeOffMin, { method: 'POST' })
    .then(res => res.json())
    .then(res => showNotification(res.msg || 'บันทึกเรียบร้อย', 'success'))
    .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
}

function setSimLevel(val) {
  document.getElementById('simTankSlider').value = val;
  document.getElementById('simTankVal').value = val;
  applySimTank(true);
}

function applySimTank(enable) {
  const level = parseFloat(document.getElementById('simTankVal').value);
  const url = '/api/sim_tank?enable=' + (enable ? '1' : '0') + '&level=' + level;
  fetch(url, { method: 'POST' })
    .then(res => res.json())
    .then(res => {
      showNotification(res.msg || (enable ? 'เปิดจำลองระดับแทงค์น้ำ' : 'ยกเลิกจำลอง'), enable ? 'success' : 'info');
      updateData();
    })
    .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
}

function applySimPool(poolType, enable, isLow) {
  const url = '/api/sim_pool?pool=' + poolType + '&enable=' + (enable ? '1' : '0') + '&low=' + (isLow ? '1' : '0');
  fetch(url, { method: 'POST' })
    .then(res => res.json())
    .then(res => {
      showNotification(res.msg || 'ดำเนินการสำเร็จ', enable ? 'success' : 'info');
      updateData();
    })
    .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
}

function saveWifi() {
  const ssid = document.getElementById('wifiSSID').value.trim();
  const pass = document.getElementById('wifiPass').value;
  if (!ssid) {
    showNotification('กรุณากรอกชื่อ Wi-Fi (SSID)', 'error');
    return;
  }
  fetch('/api/wifi?ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass), { method: 'POST' })
    .then(res => res.json())
    .then(res => showNotification(res.msg || 'บันทึกเรียบร้อย กำลังเชื่อมต่อ...', 'success'))
    .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
}

function startOTAUpdate() {
  const fileInput = document.getElementById('otaFileInput');
  if (!fileInput.files || fileInput.files.length === 0) {
    showNotification('กรุณาเลือกไฟล์เฟิร์มแวร์ .bin ก่อน', 'error');
    return;
  }

  const file = fileInput.files[0];
  if (!file.name.endsWith('.bin')) {
    showNotification('ต้องเป็นไฟล์นามสกุล .bin เท่านั้น', 'error');
    return;
  }

  if (!confirm('⚠️ ยืนยันการอัปเดตเฟิร์มแวร์?\n\n- ระบบจะหยุดการทำงานของปั๊มทั้งหมดชั่วคราว\n- กรุณาอย่าปิดเครื่องหรือตัดการเชื่อมต่อระหว่างอัปเดต')) {
    return;
  }

  const progressBox = document.getElementById('otaProgressBox');
  const progressBar = document.getElementById('otaProgressBar');
  const percentText = document.getElementById('otaPercentText');
  const statusText = document.getElementById('otaStatusText');
  const submitBtn = document.getElementById('otaSubmitBtn');

  progressBox.style.display = 'block';
  progressBar.style.width = '0%';
  percentText.innerText = '0%';
  statusText.innerText = 'กำลังส่งข้อมูลเฟิร์มแวร์...';
  statusText.style.color = '#38bdf8';
  submitBtn.disabled = true;
  submitBtn.style.opacity = '0.5';

  const formData = new FormData();
  formData.append('update', file);

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/update', true);

  xhr.upload.onprogress = function(e) {
    if (e.lengthComputable) {
      const percent = Math.round((e.loaded / e.total) * 100);
      progressBar.style.width = percent + '%';
      percentText.innerText = percent + '%';
      if (percent >= 100) {
        statusText.innerText = '⚡ กำลังเขียนลง Flash Memory และตรวจสอบความถูกต้อง...';
      }
    }
  };

  xhr.onload = function() {
    if (xhr.status === 200) {
      let res = {};
      try { res = JSON.parse(xhr.responseText); } catch(e) {}
      statusText.innerText = res.msg || '✅ อัปเดตสำเร็จ! ระบบกำลังรีบูต...';
      statusText.style.color = '#10b981';
      progressBar.style.background = '#10b981';
      showNotification('✅ อัปเดตสำเร็จ! กำลังรีบูตใน 5 วินาที...', 'success');

      let countdown = 5;
      const countInterval = setInterval(() => {
        countdown--;
        if (countdown > 0) {
          statusText.innerText = `🔄 รีบูตเสร็จสิ้น กำลังโหลดหน้าใหม่ใน ${countdown} วิ...`;
        } else {
          clearInterval(countInterval);
          window.location.reload();
        }
      }, 1000);
    } else {
      let errMsg = 'อัปเดตล้มเหลว';
      try {
        const res = JSON.parse(xhr.responseText);
        errMsg = res.msg || errMsg;
      } catch(e) {}
      statusText.innerText = '❌ ' + errMsg;
      statusText.style.color = '#ef4444';
      showNotification('❌ ' + errMsg, 'error');
      submitBtn.disabled = false;
      submitBtn.style.opacity = '1';
    }
  };

  xhr.onerror = function() {
    statusText.innerText = '❌ การเชื่อมต่อขาดหายระหว่างอัปเดต';
    statusText.style.color = '#ef4444';
    showNotification('❌ เกิดข้อผิดพลาดในการส่งข้อมูล', 'error');
    submitBtn.disabled = false;
    submitBtn.style.opacity = '1';
  };

  xhr.send(formData);
}

function saveGithubRepo() {
  const repo = document.getElementById('githubRepoInput').value.trim();
  if (!repo || !repo.includes('/')) {
    showNotification('กรุณาระบุชื่อ GitHub ในรูปแบบ username/repo เช่น autolinkmax/smartwater-master', 'error');
    return;
  }
  fetch('/api/github_config?repo=' + encodeURIComponent(repo), { method: 'POST' })
    .then(res => res.json())
    .then(res => showNotification(res.msg || 'บันทึก GitHub Repository สำเร็จ', 'success'))
    .catch(err => showNotification('เกิดข้อผิดพลาด: ' + err, 'error'));
}

let latestGithubDownloadUrl = '';
function checkGithubUpdate() {
  const btn = document.getElementById('ghCheckBtn');
  const releaseBox = document.getElementById('ghReleaseBox');
  const latestTagEl = document.getElementById('ghLatestTag');
  const releaseNameEl = document.getElementById('ghReleaseName');
  const releaseBodyEl = document.getElementById('ghReleaseBody');

  btn.disabled = true;
  btn.innerText = '⏳ กำลังตรวจสอบ GitHub API...';

  fetch('/api/github_check')
    .then(res => res.json())
    .then(data => {
      btn.disabled = false;
      btn.innerText = '🔍 ตรวจสอบเวอร์ชันล่าสุดบน GitHub';

      if (data.error) {
        showNotification(data.error, 'error');
        releaseBox.style.display = 'none';
        return;
      }

      if (data.tag) {
        latestTagEl.innerText = data.tag;
        releaseNameEl.innerText = '📦 ' + (data.name || data.tag);
        releaseBodyEl.innerText = data.body || 'ไม่มีรายละเอียด Release';
        latestGithubDownloadUrl = data.downloadUrl || '';
        releaseBox.style.display = 'block';
        showNotification('พบ Release ล่าสุด: ' + data.tag, 'success');
      }
    })
    .catch(err => {
      btn.disabled = false;
      btn.innerText = '🔍 ตรวจสอบเวอร์ชันล่าสุดบน GitHub';
      showNotification('เกิดข้อผิดพลาดในการเชื่อมต่อ GitHub: ' + err, 'error');
    });
}

function startGithubOTA() {
  if (!confirm('⚠️ ยืนยันการดาวน์โหลดและติดตั้งเฟิร์มแวร์จาก GitHub?\n\n- ESP32 จะดาวน์โหลดไฟล์ firmware.bin จาก Release ล่าสุดอัตโนมัติ\n- ระบบจะหยุดการทำงานของปั๊มทั้งหมดเพื่อความปลอดภัย\n- กรุณาอย่าปิดเครื่องจนกว่าระบบจะรีบูตเสร็จสิ้น')) {
    return;
  }

  const progressBox = document.getElementById('otaProgressBox');
  const progressBar = document.getElementById('otaProgressBar');
  const percentText = document.getElementById('otaPercentText');
  const statusText = document.getElementById('otaStatusText');
  const ghUpdateBtn = document.getElementById('ghUpdateBtn');

  progressBox.style.display = 'block';
  progressBar.style.width = '30%';
  percentText.innerText = 'ดาวน์โหลด...';
  statusText.innerText = '🌐 ESP32 กำลังดาวน์โหลดเฟิร์มแวร์จาก GitHub (autolinkmax)...';
  statusText.style.color = '#38bdf8';
  if (ghUpdateBtn) ghUpdateBtn.disabled = true;

  fetch('/api/github_update', { method: 'POST' })
    .then(res => res.json())
    .then(res => {
      if (res.status === 'OK') {
        progressBar.style.width = '100%';
        percentText.innerText = '100%';
        statusText.innerText = res.msg || '✅ อัปเดตสำเร็จ! ระบบกำลังรีบูต...';
        statusText.style.color = '#10b981';
        progressBar.style.background = '#10b981';
        showNotification('✅ อัปเดตจาก GitHub สำเร็จ! กำลังรีบูตใน 6 วินาที...', 'success');

        let countdown = 6;
        const countInterval = setInterval(() => {
          countdown--;
          if (countdown > 0) {
            statusText.innerText = `🔄 รีบูตเสร็จสิ้น กำลังเชื่อมต่อหน้าเว็บใหม่ใน ${countdown} วิ...`;
          } else {
            clearInterval(countInterval);
            window.location.reload();
          }
        }, 1000);
      } else {
        const errMsg = res.error || 'อัปเดตจาก GitHub ล้มเหลว';
        statusText.innerText = '❌ ' + errMsg;
        statusText.style.color = '#ef4444';
        showNotification('❌ ' + errMsg, 'error');
        if (ghUpdateBtn) ghUpdateBtn.disabled = false;
      }
    })
    .catch(err => {
      progressBar.style.width = '100%';
      statusText.innerText = '🔄 กำลังรีบูตระบบด้วยเฟิร์มแวร์ใหม่จาก GitHub...';
      statusText.style.color = '#10b981';
      setTimeout(() => { window.location.reload(); }, 6000);
    });
}

setInterval(updateData, 1500);
updateData();
)rawliteral";

#endif // WEB_SCRIPT_JS_H
