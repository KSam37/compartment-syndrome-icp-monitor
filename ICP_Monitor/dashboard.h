#ifndef DASHBOARD_H
#define DASHBOARD_H

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<title>ICP Monitor</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background: #f0f2f5;
    color: #1a1a2e;
    min-height: 100vh;
    overflow-x: hidden;
  }

  /* ── Header ── */
  .header {
    background: #1a1a2e;
    color: #fff;
    padding: 14px 20px;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  .header h1 { font-size: 18px; font-weight: 700; letter-spacing: 1px; }
  .battery-wrap {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 13px;
  }
  .bat-icon {
    width: 28px; height: 14px;
    border: 2px solid #fff;
    border-radius: 3px;
    position: relative;
    display: inline-block;
  }
  .bat-icon::after {
    content: '';
    position: absolute;
    right: -5px; top: 3px;
    width: 3px; height: 6px;
    background: #fff;
    border-radius: 0 2px 2px 0;
  }
  .bat-fill {
    height: 100%;
    border-radius: 1px;
    transition: width 0.5s, background 0.5s;
  }
  .bat-charging { font-size: 13px; }

  /* ── Connection Banner ── */
  .conn-banner {
    text-align: center;
    padding: 6px;
    font-size: 13px;
    font-weight: 600;
  }
  .conn-banner.ok  { background: #d4edda; color: #155724; }
  .conn-banner.err { background: #f8d7da; color: #721c24; }

  /* ── Start Overlay ── */
  .start-overlay {
    position: fixed;
    inset: 0;
    background: rgba(26,26,46,0.93);
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    z-index: 100;
  }
  .start-overlay h2 { color: #fff; font-size: 22px; margin-bottom: 8px; }
  .start-overlay p  { color: #aaa; font-size: 14px; margin-bottom: 30px; text-align: center; padding: 0 30px; }
  .start-btn {
    background: #00b894;
    color: #fff;
    border: none;
    padding: 18px 50px;
    font-size: 20px;
    font-weight: 700;
    border-radius: 12px;
    cursor: pointer;
    letter-spacing: 1px;
  }
  .start-btn:active { transform: scale(0.97); }
  .hidden { display: none !important; }

  /* ── Pressure Card ── */
  .pressure-card {
    background: #fff;
    margin: 16px;
    border-radius: 16px;
    padding: 24px 16px;
    text-align: center;
    box-shadow: 0 2px 12px rgba(0,0,0,0.06);
    transition: background 0.3s;
  }
  .pressure-val {
    font-size: 72px;
    font-weight: 800;
    line-height: 1;
    transition: color 0.3s;
    color: #27ae60;
  }
  .pressure-unit { font-size: 20px; color: #666; margin-top: 4px; }
  .status-badge {
    display: inline-block;
    margin-top: 12px;
    padding: 8px 28px;
    border-radius: 24px;
    font-size: 16px;
    font-weight: 700;
    letter-spacing: 1px;
    transition: all 0.3s;
  }
  .badge-normal   { background: #d4edda; color: #155724; }
  .badge-alert    { background: #e74c3c; color: #fff; animation: pulse 0.8s infinite alternate; }
  .badge-overrange{ background: #f39c12; color: #fff; }
  .badge-error    { background: #95a5a6; color: #fff; }
  @keyframes pulse {
    from { transform: scale(1); }
    to   { transform: scale(1.06); box-shadow: 0 0 20px rgba(231,76,60,0.5); }
  }
  .pressure-card.alert-active { background: #fde8e8; }
  .pressure-card.overrange-active { background: #fef9e7; }

  /* ── Info Row ── */
  .info-row {
    display: flex;
    justify-content: center;
    gap: 24px;
    margin: 0 16px 12px;
    font-size: 14px;
    color: #555;
  }

  /* ── Chart ── */
  .chart-card {
    background: #fff;
    margin: 0 16px 16px;
    border-radius: 16px;
    padding: 16px;
    box-shadow: 0 2px 12px rgba(0,0,0,0.06);
  }
  .chart-title { font-size: 14px; font-weight: 600; color: #555; margin-bottom: 8px; }
  canvas { width: 100% !important; height: 220px !important; display: block; }

  /* ── Controls Row (Threshold + Zero) ── */
  .controls-row {
    display: flex;
    gap: 12px;
    margin: 0 16px 16px;
    flex-wrap: wrap;
  }

  /* Threshold Card */
  .threshold-card {
    background: #fff;
    border-radius: 16px;
    padding: 14px 16px;
    box-shadow: 0 2px 12px rgba(0,0,0,0.06);
    display: flex;
    align-items: center;
    gap: 10px;
    flex: 1;
    min-width: 200px;
  }
  .threshold-label { font-size: 14px; font-weight: 600; color: #333; white-space: nowrap; }
  .threshold-controls { display: flex; align-items: center; gap: 6px; }
  .thresh-btn {
    width: 36px; height: 36px;
    border-radius: 8px;
    border: 2px solid #ddd;
    background: #f8f9fa;
    font-size: 20px;
    font-weight: 700;
    cursor: pointer;
    color: #333;
  }
  .thresh-btn:active { background: #e9ecef; }
  .thresh-input {
    width: 56px;
    text-align: center;
    font-size: 18px;
    font-weight: 700;
    border: 2px solid #ddd;
    border-radius: 8px;
    padding: 4px;
    color: #1a1a2e;
  }
  .thresh-unit { font-size: 13px; color: #666; }

  /* Zero Card */
  .zero-card {
    background: #fff;
    border-radius: 16px;
    padding: 14px 16px;
    box-shadow: 0 2px 12px rgba(0,0,0,0.06);
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 4px;
  }
  .zero-btn {
    background: #1a1a2e;
    color: #fff;
    border: none;
    padding: 10px 24px;
    font-size: 16px;
    font-weight: 700;
    border-radius: 10px;
    cursor: pointer;
    letter-spacing: 1px;
  }
  .zero-btn:active { background: #2c3e50; transform: scale(0.97); }
  .zero-label { font-size: 11px; color: #999; }

  /* ── Prototyping Panel ── */
  .proto-card {
    background: #fff;
    margin: 0 16px 16px;
    border-radius: 16px;
    box-shadow: 0 2px 12px rgba(0,0,0,0.06);
    overflow: hidden;
  }
  .proto-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 14px 18px;
    cursor: pointer;
    user-select: none;
  }
  .proto-header:active { background: #f8f9fa; }
  .proto-header-left { display: flex; align-items: center; gap: 8px; font-size: 15px; font-weight: 700; color: #1a1a2e; }
  .proto-tag { background: #e8f4fd; color: #2980b9; font-size: 10px; font-weight: 700; padding: 2px 7px; border-radius: 10px; letter-spacing: 0.5px; }
  .proto-chevron { font-size: 12px; color: #999; transition: transform 0.25s; }
  .proto-chevron.open { transform: rotate(180deg); }
  .proto-body {
    max-height: 0;
    overflow: hidden;
    transition: max-height 0.3s ease;
  }
  .proto-body.open { max-height: 600px; }
  .proto-divider { border: none; border-top: 1px solid #f0f2f5; margin: 0; }
  .proto-section { padding: 14px 18px; }
  .proto-section-title { font-size: 12px; font-weight: 700; color: #999; letter-spacing: 0.8px; text-transform: uppercase; margin-bottom: 12px; }
  .proto-row { display: flex; align-items: center; justify-content: space-between; margin-bottom: 10px; flex-wrap: wrap; gap: 6px; }
  .proto-label { font-size: 14px; color: #444; font-weight: 500; }
  .proto-sub { font-size: 12px; color: #999; }

  /* Rate buttons */
  .rate-btns { display: flex; gap: 6px; }
  .rate-btn {
    padding: 6px 14px;
    border-radius: 8px;
    border: 2px solid #ddd;
    background: #f8f9fa;
    font-size: 13px;
    font-weight: 600;
    cursor: pointer;
    color: #555;
    transition: all 0.15s;
  }
  .rate-btn.active { background: #1a1a2e; color: #fff; border-color: #1a1a2e; }
  .rate-btn:active { transform: scale(0.96); }

  /* Filter window control */
  .fwin-ctrl { display: flex; align-items: center; gap: 8px; }
  .fwin-val { font-size: 18px; font-weight: 700; min-width: 24px; text-align: center; }
  .fwin-detail { font-size: 12px; color: #999; margin-left: 2px; }

  /* Battery estimator */
  .batt-row { display: flex; align-items: center; gap: 8px; }
  .batt-mah-input {
    width: 80px;
    text-align: center;
    font-size: 16px;
    font-weight: 700;
    border: 2px solid #ddd;
    border-radius: 8px;
    padding: 5px;
    color: #1a1a2e;
  }
  .est-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 12px; }
  .est-tile {
    background: #f8f9fa;
    border-radius: 10px;
    padding: 12px;
    text-align: center;
  }
  .est-tile-val { font-size: 22px; font-weight: 800; color: #1a1a2e; }
  .est-tile-label { font-size: 11px; color: #999; margin-top: 2px; font-weight: 500; }
  .est-breakdown { font-size: 11px; color: #aaa; margin-top: 10px; text-align: center; line-height: 1.6; }

  /* ── Footer ── */
  .footer { text-align: center; padding: 10px; font-size: 11px; color: #999; }
</style>
</head>
<body>

<!-- Start Monitoring Overlay -->
<div class="start-overlay" id="startOverlay">
  <h2>ICP Monitor</h2>
  <p>Intracompartmental Pressure Monitoring System</p>
  <button class="start-btn" id="startBtn">Start Monitoring</button>
</div>

<!-- Header -->
<div class="header">
  <h1>ICP MONITOR</h1>
  <div class="battery-wrap">
    <span class="bat-charging" id="chgIcon"></span>
    <div class="bat-icon" id="batIconWrap"><div class="bat-fill" id="batFill"></div></div>
    <span id="batPct">--</span>
  </div>
</div>

<!-- Connection Banner -->
<div class="conn-banner err" id="connBanner">Connecting...</div>

<!-- Pressure Display -->
<div class="pressure-card" id="pressureCard">
  <div class="pressure-val" id="pressureVal">--</div>
  <div class="pressure-unit">mmHg</div>
  <div class="status-badge badge-normal" id="statusBadge">WAITING</div>
</div>

<!-- Info Row -->
<div class="info-row">
  <span>Temp: <strong id="tempVal">--</strong> &deg;C</span>
  <span>Sensor: <strong id="sensorStatus">--</strong></span>
</div>

<!-- Chart -->
<div class="chart-card">
  <div class="chart-title">Pressure Trend &mdash; last 60 s &nbsp;<span id="zeroLabel" style="color:#999;font-weight:400;font-size:12px;"></span></div>
  <canvas id="chart"></canvas>
</div>

<!-- Controls Row -->
<div class="controls-row">
  <!-- Threshold -->
  <div class="threshold-card">
    <div class="threshold-label">Alert&nbsp;Threshold</div>
    <div class="threshold-controls">
      <button class="thresh-btn" id="threshDown">&minus;</button>
      <input type="number" class="thresh-input" id="threshInput" value="30" min="0" max="200">
      <button class="thresh-btn" id="threshUp">+</button>
      <span class="thresh-unit">mmHg</span>
    </div>
  </div>

  <!-- Zero -->
  <div class="zero-card">
    <button class="zero-btn" id="zeroBtn">ZERO</button>
    <div class="zero-label">Set atmospheric = 0</div>
  </div>
</div>

<!-- Prototyping Options Panel -->
<div class="proto-card">
  <div class="proto-header" id="protoToggle">
    <div class="proto-header-left">
      <span>Prototyping Options</span>
      <span class="proto-tag">DEV</span>
    </div>
    <span class="proto-chevron" id="protoChevron">&#9660;</span>
  </div>
  <div class="proto-body" id="protoBody">
    <hr class="proto-divider">

    <!-- Signal Settings -->
    <div class="proto-section">
      <div class="proto-section-title">Signal Settings</div>

      <div class="proto-row">
        <span class="proto-label">Sample Rate</span>
        <div class="rate-btns">
          <button class="rate-btn" data-hz="0.1">0.1 Hz</button>
          <button class="rate-btn" data-hz="1">1 Hz</button>
          <button class="rate-btn" data-hz="5">5 Hz</button>
          <button class="rate-btn active" data-hz="10">10 Hz</button>
          <button class="rate-btn" data-hz="20">20 Hz</button>
        </div>
      </div>

      <div class="proto-row">
        <span class="proto-label">Median Window</span>
        <div class="fwin-ctrl">
          <button class="thresh-btn" id="fwinDown">&minus;</button>
          <span class="fwin-val" id="fwinVal">5</span>
          <button class="thresh-btn" id="fwinUp">+</button>
          <span class="fwin-detail" id="fwinMs">samples &nbsp;|&nbsp; 500 ms</span>
        </div>
      </div>
    </div>

    <hr class="proto-divider">

    <!-- Battery Estimator -->
    <div class="proto-section">
      <div class="proto-section-title">Battery Life Estimator</div>
      <div class="proto-row">
        <span class="proto-label">Battery Capacity</span>
        <div class="batt-row">
          <input type="number" class="batt-mah-input" id="battMah" value="1000" min="50" max="10000">
          <span class="proto-sub">mAh</span>
        </div>
      </div>
      <div class="est-grid">
        <div class="est-tile">
          <div class="est-tile-val" id="estMa">--</div>
          <div class="est-tile-label">Est. Current Draw</div>
        </div>
        <div class="est-tile">
          <div class="est-tile-val" id="estLife">--</div>
          <div class="est-tile-label">Est. Battery Life</div>
        </div>
      </div>
      <div class="est-breakdown" id="estBreakdown"></div>
    </div>
  </div>
</div>

<div class="footer">ICP Monitor &bull; Compartment Syndrome Device</div>

<script>
(function() {
  // ── State ──
  let threshold = parseInt(localStorage.getItem('icpThreshold')) || 30;
  let audioCtx = null;
  let ws = null;
  const MAX_POINTS = 600;  // 60 seconds at 10 Hz
  let dataBuffer = [];
  let lastBeep = 0;

  // ── DOM ──
  const $ = id => document.getElementById(id);
  const pressureVal  = $('pressureVal');
  const statusBadge  = $('statusBadge');
  const pressureCard = $('pressureCard');
  const tempVal      = $('tempVal');
  const sensorStatus = $('sensorStatus');
  const connBanner   = $('connBanner');
  const batFill      = $('batFill');
  const batPct       = $('batPct');
  const chgIcon      = $('chgIcon');
  const threshInput  = $('threshInput');
  const canvas       = $('chart');
  const ctx          = canvas.getContext('2d');
  const zeroLabel    = $('zeroLabel');

  threshInput.value = threshold;

  // ── Start Button (unlocks iOS audio) ──
  $('startBtn').addEventListener('click', function() {
    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    const osc = audioCtx.createOscillator();
    const gain = audioCtx.createGain();
    gain.gain.value = 0;
    osc.connect(gain);
    gain.connect(audioCtx.destination);
    osc.start();
    osc.stop(audioCtx.currentTime + 0.1);
    $('startOverlay').classList.add('hidden');
    connectWS();
  });

  // ── Zero Button ──
  $('zeroBtn').addEventListener('click', function() {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({cmd: 'zero'}));
      zeroLabel.textContent = '(zeroed)';
      setTimeout(() => zeroLabel.textContent = '', 3000);
    }
  });

  // ── Threshold Controls ──
  $('threshDown').addEventListener('click', () => setThreshold(threshold - 5));
  $('threshUp').addEventListener('click',   () => setThreshold(threshold + 5));
  threshInput.addEventListener('change',    () => setThreshold(parseInt(threshInput.value) || 30));

  function setThreshold(val) {
    threshold = Math.max(0, Math.min(200, val));
    threshInput.value = threshold;
    localStorage.setItem('icpThreshold', threshold);
  }

  // ── WebSocket ──
  function connectWS() {
    ws = new WebSocket('ws://' + location.host + '/ws');
    ws.onopen = () => {
      connBanner.textContent = 'Connected';
      connBanner.className = 'conn-banner ok';
      // Expose send function so the prototyping panel can share this socket
      window._icpWsSend = (msg) => { if (ws.readyState === WebSocket.OPEN) ws.send(msg); };
    };
    ws.onclose = () => {
      connBanner.textContent = 'Disconnected \u2014 Reconnecting...';
      connBanner.className = 'conn-banner err';
      setTimeout(connectWS, 2000);
    };
    ws.onerror = () => ws.close();
    ws.onmessage = (evt) => {
      try {
        const d = JSON.parse(evt.data);
        updateDisplay(d);
        updateChart(d);
        checkAlert(d);
        // Keep prototyping panel in sync with actual ESP32 settings
        if (d.rate != null && d.fwin != null && window._icpProtoSync) {
          window._icpProtoSync(d.rate, d.fwin);
        }
      } catch(e) {}
    };
  }

  // ── Display Update ──
  function updateDisplay(d) {
    // Overrange takes priority
    if (d.ovr) {
      pressureVal.textContent = 'OVR';
      pressureVal.style.color = '#f39c12';
      pressureCard.classList.remove('alert-active');
      pressureCard.classList.add('overrange-active');
      statusBadge.textContent = 'SENSOR OVERRANGE';
      statusBadge.className = 'status-badge badge-overrange';
      sensorStatus.textContent = 'Overrange';
      sensorStatus.style.color = '#f39c12';
      return;
    }

    pressureCard.classList.remove('overrange-active');

    if (!d.ok) {
      pressureVal.textContent = '--';
      pressureVal.style.color = '#95a5a6';
      statusBadge.textContent = 'SENSOR ERROR';
      statusBadge.className = 'status-badge badge-error';
      sensorStatus.textContent = 'Error';
      sensorStatus.style.color = '#e74c3c';
      return;
    }

    pressureVal.textContent = d.p.toFixed(1);
    sensorStatus.textContent = 'OK';
    sensorStatus.style.color = '#27ae60';
    tempVal.textContent = (d.t != null) ? d.t.toFixed(1) : '--';

    // Battery
    if (d.bat === -1) {
      batPct.textContent = 'USB';
      batFill.style.width = '100%';
      batFill.style.background = '#3498db';
      chgIcon.textContent = '\u26A1';
    } else {
      batPct.textContent = d.bat + '%';
      const pct = Math.max(0, Math.min(100, d.bat));
      batFill.style.width = pct + '%';
      batFill.style.background = pct > 30 ? '#27ae60' : pct > 10 ? '#f39c12' : '#e74c3c';
      chgIcon.textContent = d.chg ? '\u26A1' : '';
    }
  }

  // ── Alert ──
  function checkAlert(d) {
    if (!d.ok || d.ovr || d.p == null) return;
    if (d.p >= threshold) {
      pressureVal.style.color = '#e74c3c';
      pressureCard.classList.add('alert-active');
      statusBadge.textContent = 'ALERT \u2014 HIGH PRESSURE';
      statusBadge.className = 'status-badge badge-alert';
      const now = Date.now();
      if (now - lastBeep > 2000) { playBeep(); lastBeep = now; }
    } else {
      pressureVal.style.color = '#27ae60';
      pressureCard.classList.remove('alert-active');
      statusBadge.textContent = 'NORMAL';
      statusBadge.className = 'status-badge badge-normal';
    }
  }

  function playBeep() {
    if (!audioCtx) return;
    try {
      const osc = audioCtx.createOscillator();
      const gain = audioCtx.createGain();
      osc.frequency.value = 880;
      gain.gain.value = 0.5;
      osc.connect(gain);
      gain.connect(audioCtx.destination);
      osc.start();
      gain.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + 0.3);
      osc.stop(audioCtx.currentTime + 0.35);
    } catch(e) {}
  }

  // ── Chart ──
  function resizeCanvas() {
    const dpr = window.devicePixelRatio || 1;
    const rect = canvas.getBoundingClientRect();
    canvas.width  = rect.width  * dpr;
    canvas.height = rect.height * dpr;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    drawChart();
  }
  window.addEventListener('resize', resizeCanvas);
  resizeCanvas();

  function updateChart(d) {
    if (!d.ok || d.ovr || d.p == null) return;
    dataBuffer.push(d.p);
    if (dataBuffer.length > MAX_POINTS) dataBuffer.shift();
    drawChart();
  }

  function drawChart() {
    const W = canvas.getBoundingClientRect().width;
    const H = canvas.getBoundingClientRect().height;
    const pad = { top: 10, right: 60, bottom: 28, left: 42 };
    const cw = W - pad.left - pad.right;
    const ch = H - pad.top - pad.bottom;

    ctx.clearRect(0, 0, W, H);

    // Y-axis auto scale
    let yMax = Math.max(threshold + 10, 50);
    if (dataBuffer.length > 0) {
      const maxP = Math.max(...dataBuffer);
      if (maxP > yMax - 5) yMax = maxP + 15;
    }
    const yMin = 0;

    // Grid lines
    ctx.strokeStyle = '#e9ecef';
    ctx.lineWidth = 1;
    for (let i = 0; i <= 5; i++) {
      const y = pad.top + (ch / 5) * i;
      ctx.beginPath(); ctx.moveTo(pad.left, y); ctx.lineTo(W - pad.right, y); ctx.stroke();
      const val = yMax - ((yMax - yMin) / 5) * i;
      ctx.fillStyle = '#999';
      ctx.font = '11px sans-serif';
      ctx.textAlign = 'right';
      ctx.fillText(val.toFixed(0), pad.left - 6, y + 4);
    }

    // X-axis labels: 60s window, mark every 10s
    ctx.fillStyle = '#999';
    ctx.font = '11px sans-serif';
    ctx.textAlign = 'center';
    const xTicks = ['0:60','0:50','0:40','0:30','0:20','0:10','0:00'];
    for (let i = 0; i < xTicks.length; i++) {
      const x = pad.left + (cw / (xTicks.length - 1)) * i;
      ctx.fillText(xTicks[i], x, H - 6);
    }

    // Threshold line
    const threshY = pad.top + ch * (1 - (threshold - yMin) / (yMax - yMin));
    ctx.strokeStyle = '#e74c3c';
    ctx.lineWidth = 1.5;
    ctx.setLineDash([6, 4]);
    ctx.beginPath();
    ctx.moveTo(pad.left, threshY);
    ctx.lineTo(W - pad.right, threshY);
    ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = '#e74c3c';
    ctx.font = '10px sans-serif';
    ctx.textAlign = 'left';
    ctx.fillText(threshold + ' mmHg', W - pad.right + 4, threshY + 4);

    // Data line
    if (dataBuffer.length < 2) return;
    ctx.strokeStyle = '#3498db';
    ctx.lineWidth = 2;
    ctx.lineJoin = 'round';
    ctx.beginPath();
    for (let i = 0; i < dataBuffer.length; i++) {
      const x = pad.left + (cw / (MAX_POINTS - 1)) * (MAX_POINTS - dataBuffer.length + i);
      const y = pad.top + ch * (1 - (dataBuffer[i] - yMin) / (yMax - yMin));
      if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.stroke();

    // Fill under curve
    const lastX  = pad.left + (cw / (MAX_POINTS - 1)) * (MAX_POINTS - 1);
    const firstX = pad.left + (cw / (MAX_POINTS - 1)) * (MAX_POINTS - dataBuffer.length);
    ctx.lineTo(lastX, pad.top + ch);
    ctx.lineTo(firstX, pad.top + ch);
    ctx.closePath();
    ctx.fillStyle = 'rgba(52, 152, 219, 0.08)';
    ctx.fill();
  }
})();

// ── Prototyping Panel ──
(function() {
  const $ = id => document.getElementById(id);

  let currentRateHz = 10;
  let currentFwin   = 5;

  // Collapse / expand
  $('protoToggle').addEventListener('click', function() {
    const body    = $('protoBody');
    const chevron = $('protoChevron');
    const open    = body.classList.toggle('open');
    chevron.classList.toggle('open', open);
    if (open) calcPower();
  });

  // ── Rate buttons ──
  document.querySelectorAll('.rate-btn').forEach(btn => {
    btn.addEventListener('click', function() {
      const hz = parseInt(this.dataset.hz);
      document.querySelectorAll('.rate-btn').forEach(b => b.classList.remove('active'));
      this.classList.add('active');
      currentRateHz = hz;
      sendCmd('setRate', hz);
      calcPower();
    });
  });

  // ── Filter window stepper (+2 / -2 to keep odd) ──
  function updateFwinLabel(n) {
    const ms = Math.round((n / currentRateHz) * 1000);
    $('fwinMs').textContent = `samples\u00a0|\u00a0${ms} ms`;
  }

  $('fwinDown').addEventListener('click', function() {
    if (currentFwin > 1) {
      currentFwin = Math.max(1, currentFwin - 2);
      $('fwinVal').textContent = currentFwin;
      updateFwinLabel(currentFwin);
      sendCmd('setFilter', currentFwin);
    }
  });
  $('fwinUp').addEventListener('click', function() {
    if (currentFwin < 21) {
      currentFwin = Math.min(21, currentFwin + 2);
      $('fwinVal').textContent = currentFwin;
      updateFwinLabel(currentFwin);
      sendCmd('setFilter', currentFwin);
    }
  });

  // ── Battery estimator ──
  // Power model (XIAO ESP32-C5, WiFi AP mode, WebSocket streaming):
  //   WiFi AP base draw : 130 mA  (radio always on)
  //   CPU active        :  20 mA  (main loop, JSON encode, WS send)
  //   Per-Hz overhead   :   0.3 mA  (I2C read + filter compute per sample/s)
  function calcPower() {
    const battMah   = parseFloat($('battMah').value) || 1000;
    const wifiMa    = 130;
    const cpuMa     = 20;
    const sampleMa  = parseFloat((currentRateHz * 0.3).toFixed(1));
    const totalMa   = wifiMa + cpuMa + sampleMa;

    const lifeH_raw = battMah / totalMa;
    const lifeH     = Math.floor(lifeH_raw);
    const lifeM     = Math.round((lifeH_raw - lifeH) * 60);
    const lifeStr   = lifeH > 0 ? `${lifeH}h ${lifeM}m` : `${lifeM}m`;

    $('estMa').textContent   = totalMa.toFixed(0) + ' mA';
    $('estLife').textContent = lifeStr;
    $('estBreakdown').innerHTML =
      `WiFi AP: ${wifiMa} mA &nbsp;+&nbsp; CPU: ${cpuMa} mA &nbsp;+&nbsp; Sampling @ ${currentRateHz} Hz: ${sampleMa} mA<br>`+
      `<em>Note: WiFi radio dominates; rate has minor effect on total draw.</em>`;
  }

  $('battMah').addEventListener('input', calcPower);
  calcPower();

  // ── Helper to send commands via the main WS (shared via closure bridge) ──
  function sendCmd(cmd, value) {
    // Access the WebSocket from the outer closure via window-level bridge
    if (window._icpWsSend) window._icpWsSend(JSON.stringify({cmd, value}));
  }

  // Expose sync function — rate arrives as tenths-of-Hz integer
  window._icpProtoSync = function(rateTenths, fwin) {
    const rateHz = rateTenths / 10.0;
    currentRateHz = rateHz;
    currentFwin   = fwin;
    document.querySelectorAll('.rate-btn').forEach(btn => {
      btn.classList.toggle('active', parseFloat(btn.dataset.hz) === rateHz);
    });
    $('fwinVal').textContent = fwin;
    updateFwinLabel(fwin);
    calcPower();
  };
})();
</script>
</body>
</html>
)rawliteral";

#endif
