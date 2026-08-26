#ifndef WEB_STYLE_CSS_H
#define WEB_STYLE_CSS_H

#include <Arduino.h>

const char DASHBOARD_CSS[] PROGMEM = R"rawliteral(
:root {
  --bg-dark: #090e17;
  --card-bg: rgba(18, 26, 43, 0.75);
  --card-border: rgba(255, 255, 255, 0.08);
  --accent-blue: #0284c7;
  --accent-cyan: #06b6d4;
  --accent-green: #10b981;
  --accent-yellow: #f59e0b;
  --accent-red: #ef4444;
  --text-main: #f8fafc;
  --text-muted: #94a3b8;
}

* { box-sizing: border-box; margin: 0; padding: 0; font-family: 'Prompt', -apple-system, BlinkMacSystemFont, sans-serif; }
body {
  background: var(--bg-dark);
  color: var(--text-main);
  min-height: 100vh;
  padding: 16px;
  max-width: 1200px;
  margin: 0 auto;
  -webkit-font-smoothing: antialiased;
}

/* Header */
.header {
  display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 12px;
  background: var(--card-bg); backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px);
  padding: 14px 20px; border-radius: 16px; border: 1px solid var(--card-border);
  margin-bottom: 16px;
}
.header h1 { font-size: 1.25rem; font-weight: 600; color: var(--accent-cyan); display: flex; align-items: center; gap: 8px; }
.status-badge {
  padding: 6px 14px; border-radius: 20px; font-size: 0.85rem; font-weight: 500;
  background: rgba(16, 185, 129, 0.15); color: var(--accent-green); border: 1px solid rgba(16, 185, 129, 0.3);
  white-space: nowrap;
}
.status-badge.error { background: rgba(239, 68, 68, 0.15); color: var(--accent-red); border-color: var(--accent-red); }

/* Global Alert Banner */
#alertBanner {
  display: none; padding: 12px 16px; border-radius: 12px; margin-bottom: 16px;
  font-size: 0.9rem; font-weight: 500; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 10px;
}
#alertBanner.show-err { display: flex; background: rgba(239, 68, 68, 0.18); border: 1px solid #ef4444; color: #fca5a5; }
#alertBanner.show-warn { display: flex; background: rgba(245, 158, 11, 0.18); border: 1px solid #f59e0b; color: #fde68a; }

/* Navigation Tabs (Mobile Scrollable & Tablet/Desktop Wrap) */
.tab-nav {
  display: flex; gap: 8px; margin-bottom: 16px; overflow-x: auto; padding-bottom: 6px;
  scrollbar-width: thin; -webkit-overflow-scrolling: touch;
}
.tab-nav::-webkit-scrollbar { height: 4px; }
.tab-nav::-webkit-scrollbar-thumb { background: rgba(255,255,255,0.15); border-radius: 4px; }

.tab-btn {
  background: rgba(30, 41, 59, 0.6); color: var(--text-muted); border: 1px solid var(--card-border);
  padding: 10px 16px; border-radius: 12px; font-size: 0.9rem; font-weight: 500;
  cursor: pointer; transition: all 0.2s; white-space: nowrap; display: flex; align-items: center; gap: 6px;
  flex-shrink: 0;
}
.tab-btn:hover { background: rgba(51, 65, 85, 0.8); color: var(--text-main); }
.tab-btn.active {
  background: var(--accent-blue); color: white; border-color: #38bdf8;
  box-shadow: 0 4px 12px rgba(2, 132, 199, 0.3);
}

.tab-pane { display: none; }
.tab-pane.active { display: block; animation: fadeIn 0.25s ease-out; }
@keyframes fadeIn { from { opacity: 0; transform: translateY(4px); } to { opacity: 1; transform: translateY(0); } }

/* Grid & Cards Responsive (Auto-Fit for PC/Tablet, Full Width on Mobile) */
.grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
  gap: 16px;
}
.card {
  background: var(--card-bg); backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px);
  border: 1px solid var(--card-border); border-radius: 16px; padding: 20px;
}
.card-title {
  font-size: 1rem; font-weight: 600; color: var(--text-muted); margin-bottom: 16px;
  display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 6px;
}

/* Tank Level Gauge */
.tank-container { display: flex; align-items: center; gap: 20px; flex-wrap: wrap; }
.tank-graphic {
  width: 80px; height: 140px; border: 3px solid #38bdf8; border-radius: 12px;
  position: relative; overflow: hidden; background: rgba(15, 23, 42, 0.6); flex-shrink: 0;
}
.tank-water {
  position: absolute; bottom: 0; left: 0; right: 0;
  background: linear-gradient(180deg, #38bdf8, #0284c7);
  transition: height 0.8s ease-in-out;
}
.tank-value { font-size: 2.2rem; font-weight: 700; color: var(--text-main); }
.tank-sub { font-size: 0.85rem; color: var(--text-muted); }

/* Relays */
.relay-item {
  display: flex; justify-content: space-between; align-items: center;
  padding: 10px 14px; background: rgba(15, 23, 42, 0.4); border-radius: 10px; margin-bottom: 8px;
}
.relay-tag {
  width: 10px; height: 10px; border-radius: 50%; background: #475569; display: inline-block; margin-right: 8px;
}
.relay-tag.active { background: var(--accent-green); box-shadow: 0 0 10px var(--accent-green); }

/* Buttons & Quick Controls (Touch Friendly) */
.btn-group {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
  gap: 10px;
  margin-top: 10px;
}
.btn {
  background: #334155; color: white; border: none; padding: 12px 16px; border-radius: 10px;
  font-size: 0.92rem; font-weight: 500; cursor: pointer; transition: all 0.2s;
  display: inline-flex; align-items: center; justify-content: center; gap: 6px;
  text-align: center; user-select: none; -webkit-tap-highlight-color: transparent;
}
.btn:hover { background: #475569; }
.btn.primary { background: #0284c7; }
.btn.primary:hover { background: #0369a1; }
.btn.success { background: #059669; }
.btn.success:hover { background: #047857; }
.btn.danger { background: #dc2626; font-weight: 600; }
.btn.danger:hover { background: #b91c1c; }

/* Telemetry Pill */
.pill-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 10px; }
.pill {
  background: rgba(15, 23, 42, 0.5); padding: 12px; border-radius: 12px; border: 1px solid var(--card-border);
}
.pill-label { font-size: 0.75rem; color: var(--text-muted); }
.pill-val { font-size: 1.2rem; font-weight: 600; margin-top: 4px; }

/* Inputs */
.input-row { margin-bottom: 12px; }
.input-row label { display: block; font-size: 0.85rem; color: var(--text-muted); margin-bottom: 4px; }
.input-row input, .input-row select {
  width: 100%; padding: 10px 12px; background: #0f172a;
  border: 1px solid var(--card-border); border-radius: 8px; color: #ffffff; font-size: 0.95rem;
}
.input-row select option {
  background: #0f172a; color: #ffffff;
}

/* Smart Schedule Slot Card - Modern Smart Home Design */
.sched-card {
  background: #0d1527; border: 1px solid #1e293b;
  border-radius: 14px; padding: 18px 20px; margin-bottom: 14px; position: relative;
  transition: all 0.25s ease-in-out;
}
.sched-card.active-slot {
  border: 1.5px solid #10b981;
  box-shadow: 0 0 15px rgba(16, 185, 129, 0.15);
}
.sched-header {
  display: flex; justify-content: space-between; align-items: center; margin-bottom: 14px; flex-wrap: wrap; gap: 8px;
}

.time-picker-row {
  display: grid;
  grid-template-columns: 1.3fr 1fr 1fr auto;
  gap: 12px;
  align-items: flex-end;
}

.sched-field-label {
  font-size: 0.8rem; color: #94a3b8; margin-bottom: 6px; font-weight: 600;
}
.sched-input-box {
  background: #151f38; border: 1px solid #243456; border-radius: 10px;
  color: white; padding: 6px 10px; font-size: 0.95rem; display: flex; align-items: center; gap: 6px; width: 100%;
}
.sched-input-box select {
  background: #151f38; color: #ffffff; font-size: 0.95rem; font-weight: 600;
  border: 1px solid transparent; border-radius: 6px; padding: 4px 6px;
  outline: none; cursor: pointer;
}
.sched-input-box select option {
  background: #0f172a; color: #ffffff; padding: 8px 12px; font-size: 0.95rem;
}

.time-select-group {
  display: flex; align-items: center; gap: 4px; width: 100%;
}
.time-select-group select {
  flex: 1; background: #0f172a; border: 1px solid #334155; color: #ffffff;
  padding: 8px 4px; border-radius: 8px; font-family: monospace; font-size: 0.95rem;
  font-weight: 700; text-align: center; cursor: pointer; outline: none;
}
.time-select-group select:focus {
  border-color: #10b981;
}
.time-select-sep {
  font-weight: 700; color: #38bdf8; font-size: 1.1rem;
}

.sched-dur-badge {
  background: rgba(16, 185, 129, 0.15); border: 1px solid rgba(16, 185, 129, 0.4);
  color: #10b981; padding: 10px 16px; border-radius: 20px; font-size: 0.9rem; font-weight: 700;
  display: inline-flex; align-items: center; justify-content: center; gap: 6px; white-space: nowrap; height: 44px;
}
.sched-dur-badge.disabled {
  background: rgba(100, 116, 139, 0.15); border-color: #334155; color: #64748b;
}

.btn-add-sched {
  background: transparent; border: 2px dashed #10b981; color: #10b981;
  width: 100%; padding: 14px; border-radius: 12px; font-size: 0.95rem; font-weight: 600;
  cursor: pointer; display: flex; align-items: center; justify-content: center; gap: 8px;
  transition: all 0.2s; margin-bottom: 12px;
}
.btn-add-sched:hover {
  background: rgba(16, 185, 129, 0.1);
}
.btn-save-sched {
  background: #10b981; color: #022c22; width: 100%; padding: 14px; border-radius: 12px;
  font-size: 1rem; font-weight: 700; border: none; cursor: pointer; display: flex;
  align-items: center; justify-content: center; gap: 8px; transition: all 0.2s;
}
.btn-save-sched:hover {
  background: #059669; color: white;
}

/* Switch Toggle */
.switch {
  position: relative; display: inline-block; width: 56px; height: 30px; flex-shrink: 0;
}
.switch input { opacity: 0; width: 0; height: 0; }
.slider {
  position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
  background-color: #334155; transition: .3s; border-radius: 30px;
  border: 1px solid var(--card-border);
}
.slider:before {
  position: absolute; content: ""; height: 22px; width: 22px; left: 3px; bottom: 3px;
  background-color: white; transition: .3s; border-radius: 50%;
}
input:checked + .slider {
  background-color: #0284c7; box-shadow: 0 0 12px rgba(2, 132, 199, 0.6);
}
input:checked + .slider:before {
  transform: translateX(26px);
}

/* Notification Label / Toast */
#notifyToast {
  position: fixed; bottom: 24px; right: 24px; z-index: 9999;
  padding: 14px 20px; border-radius: 12px; font-weight: 500; font-size: 0.95rem;
  color: white; background: #1e293b; border: 1px solid var(--card-border);
  box-shadow: 0 10px 25px rgba(0,0,0,0.5);
  display: flex; align-items: center; gap: 10px;
  opacity: 0; transform: translateY(20px); transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
  pointer-events: none; max-width: 90vw;
}
#notifyToast.show {
  opacity: 1; transform: translateY(0); pointer-events: auto;
}
#notifyToast.success { background: #064e3b; border-color: #059669; }
#notifyToast.error { background: #7f1d1d; border-color: #dc2626; }
#notifyToast.info { background: #0c4a6e; border-color: #0284c7; }

/* Action Modal for Duration */
#durModal {
  display: none; position: fixed; inset: 0; background: rgba(0,0,0,0.7);
  backdrop-filter: blur(4px); -webkit-backdrop-filter: blur(4px); z-index: 999; align-items: center; justify-content: center;
  padding: 16px;
}
#durModal.active { display: flex; }
.modal-box {
  background: #1e293b; border: 1px solid var(--card-border); border-radius: 16px;
  padding: 24px; width: 100%; max-width: 380px; box-shadow: 0 20px 40px rgba(0,0,0,0.6);
}
.modal-title { font-size: 1.15rem; font-weight: 600; margin-bottom: 12px; color: var(--accent-cyan); }

/* ========================================================= */
/* RESPONSIVE MEDIA QUERIES (Mobile / Tablet / PC)           */
/* ========================================================= */

/* Tablet (Max 992px) */
@media (max-width: 992px) {
  body { padding: 12px; }
  .grid { grid-template-columns: 1fr 1fr; }
  .card[style*="grid-column: span 2"] { grid-column: span 2 !important; }
}

/* Mobile Devices (Max 768px) */
@media (max-width: 768px) {
  body { padding: 8px; }
  .header { padding: 12px 14px; margin-bottom: 12px; border-radius: 12px; }
  .header h1 { font-size: 1.1rem; }
  .status-badge { font-size: 0.8rem; padding: 4px 10px; }
  
  .grid { grid-template-columns: 1fr; gap: 12px; }
  .card { padding: 16px; border-radius: 14px; }
  .card[style*="grid-column: span 2"] { grid-column: span 1 !important; }
  
  .btn-group { grid-template-columns: 1fr 1fr; gap: 8px; }
  .btn { padding: 10px 12px; font-size: 0.85rem; }
  
  /* Schedule Card on Mobile */
  .time-picker-row {
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .sched-dur-badge {
    grid-column: span 2;
    width: 100%;
    height: 38px;
  }
  
  #notifyToast {
    left: 12px; right: 12px; bottom: 16px;
    font-size: 0.85rem; padding: 10px 14px;
  }
}

/* Small Mobile (Max 480px) */
@media (max-width: 480px) {
  .header { flex-direction: column; align-items: flex-start; gap: 8px; }
  .btn-group { grid-template-columns: 1fr; }
  .time-picker-row { grid-template-columns: 1fr; }
  .sched-dur-badge { grid-column: span 1; }
  .tank-container { justify-content: center; }
}
)rawliteral";

#endif // WEB_STYLE_CSS_H
