#include "WebUiServer.h"

#include <WiFi.h>
#include <cstring>
#include <cstdlib>

#include "DebugLogger.h"

namespace babot {

namespace {

const char kIndexHtml[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>BaBot Control</title>
  <style>
    :root { color-scheme: dark; --bg:#0f1720; --panel:#182433; --accent:#4ecdc4; --warn:#ffbf69; --text:#edf2f7; --muted:#94a3b8; }
    body { margin:0; font-family:Verdana, Geneva, Tahoma, sans-serif; background:radial-gradient(circle at top,#1f3b57,#0f1720 60%); color:var(--text); }
    .wrap { max-width:1200px; margin:0 auto; padding:20px; }
    h1 { margin:0 0 8px; font-size:28px; }
    .sub { color:var(--muted); margin-bottom:18px; }
    .grid { display:grid; grid-template-columns:1.2fr 1fr; gap:16px; }
    .panel { background:rgba(24,36,51,.92); border:1px solid rgba(255,255,255,.08); border-radius:18px; padding:16px; box-shadow:0 18px 40px rgba(0,0,0,.25); }
    .metrics { display:grid; grid-template-columns:repeat(3,1fr); gap:12px; margin-top:14px; }
    .metric { background:rgba(255,255,255,.04); border-radius:12px; padding:12px; }
    .metric .label { color:var(--muted); font-size:12px; }
    .metric .value { font-size:22px; margin-top:4px; }
    canvas { width:100%; aspect-ratio:1/1; height:auto; max-height:min(72vh,640px); background:linear-gradient(180deg,#13202d,#0c1218); border-radius:18px; border:1px solid rgba(255,255,255,.08); display:block; }
    .vizTitle { margin:14px 0 8px; color:var(--muted); font-size:13px; }
    .vizOverlay { display:block; margin:0 0 14px; }
    .pipCanvas { width:100%; height:auto; max-height:min(72vh,640px); aspect-ratio:1/1; background:linear-gradient(180deg,#13202d,#0c1218); border-radius:18px; border:1px solid rgba(255,255,255,.08); box-shadow:0 12px 24px rgba(0,0,0,.32); }
    .controls { display:grid; grid-template-columns:repeat(2,1fr); gap:12px; }
    .field { display:flex; flex-direction:column; gap:6px; }
    .field label { color:var(--muted); font-size:13px; }
    .field input { border-radius:10px; border:1px solid rgba(255,255,255,.14); background:#0f1720; color:var(--text); padding:10px 12px; font-size:15px; }
    .actions, .modes { display:flex; flex-wrap:wrap; gap:10px; margin-top:14px; }
    .drawer { margin-top:12px; border:1px solid rgba(255,255,255,.08); border-radius:14px; background:rgba(255,255,255,.035); overflow:hidden; }
    .drawer summary { cursor:pointer; padding:12px 14px; font-weight:700; color:var(--text); list-style:none; }
    .drawer summary::-webkit-details-marker { display:none; }
    .drawer summary::after { content:'+'; float:right; color:var(--accent); font-size:18px; line-height:1; }
    .drawer[open] summary::after { content:'-'; }
    .drawerBody { padding:0 14px 14px; }
    .drawerBody .actions, .drawerBody .modes, .drawerBody .controls { margin-top:0; }
    .drawerGroup { display:flex; flex-wrap:wrap; gap:10px; margin-top:12px; padding-top:12px; border-top:1px solid rgba(255,255,255,.07); }
    .drawerGroup:first-child { margin-top:0; padding-top:0; border-top:0; }
    .runtimeActions { display:flex; flex-wrap:wrap; gap:10px; margin-bottom:14px; }
    .autotuneGrid { display:grid; grid-template-columns:repeat(2,1fr); gap:10px; margin-top:12px; }
    .autotuneMetric { background:rgba(255,255,255,.04); border-radius:12px; padding:10px 12px; border:1px solid rgba(255,255,255,.06); }
    .autotuneMetric .label { color:var(--muted); font-size:12px; }
    .autotuneMetric .value { margin-top:4px; font-size:18px; }
    button { border:0; border-radius:999px; padding:10px 16px; font-weight:700; cursor:pointer; background:var(--accent); color:#092126; }
    button.alt { background:#334155; color:var(--text); }
    button.active { box-shadow:0 0 0 2px rgba(78,205,196,.5) inset; background:#4ecdc4; color:#092126; }
    button:disabled { opacity:.45; cursor:not-allowed; }
    .banner { margin-top:14px; border-radius:14px; padding:12px 14px; font-weight:700; }
    .banner.ready { background:rgba(78,205,196,.12); border:1px solid rgba(78,205,196,.35); color:#b4f5ef; }
    .banner.warn { background:rgba(255,191,105,.12); border:1px solid rgba(255,191,105,.35); color:#ffd79b; }
    .banner.fail { background:rgba(255,107,107,.12); border:1px solid rgba(255,107,107,.35); color:#ffc3c3; }
    .systemSummary { margin-top:10px; color:var(--muted); font-size:13px; min-height:18px; }
    .healthList { display:grid; gap:10px; }
    .healthItem { background:rgba(255,255,255,.04); border-radius:12px; padding:10px 12px; border:1px solid rgba(255,255,255,.06); }
    .healthHead { display:flex; justify-content:space-between; gap:10px; align-items:center; font-size:13px; }
    .pill { padding:4px 10px; border-radius:999px; font-size:11px; font-weight:700; letter-spacing:.03em; }
    .pill.ok { background:rgba(78,205,196,.15); color:#9ef0e8; }
    .pill.warn { background:rgba(255,191,105,.18); color:#ffd79b; }
    .pill.fail { background:rgba(255,107,107,.18); color:#ffc3c3; }
    .pill.off { background:rgba(148,163,184,.16); color:#cbd5e1; }
    .detail { margin-top:6px; color:var(--muted); font-size:12px; line-height:1.4; }
    .status { margin-top:12px; color:var(--warn); min-height:20px; }
    @media (max-width: 900px) { .grid, .controls, .metrics, .autotuneGrid { grid-template-columns:1fr; } }
  </style>
</head>
<body>
  <div class="wrap">
    <h1>BaBot ESP32-S3 Console</h1>
    <div class="sub">Access point: <strong>12345678</strong> | Open the page while connected to the robot network.</div>
    <div class="grid">
      <section class="panel">
        <canvas id="ballCanvas" width="640" height="640"></canvas>
        <div class="metrics">
          <div class="metric"><div class="label">Mode</div><div class="value" id="modeValue">-</div></div>
          <div class="metric"><div class="label">Ball</div><div class="value" id="ballValue">-</div></div>
          <div class="metric"><div class="label">Digipot</div><div class="value" id="digipotValue">-</div></div>
          <div class="metric"><div class="label">Center X</div><div class="value" id="centerXValue">-</div></div>
          <div class="metric"><div class="label">Center Y</div><div class="value" id="centerYValue">-</div></div>
          <div class="metric"><div class="label">Error X</div><div class="value" id="errorXValue">-</div></div>
          <div class="metric"><div class="label">Error Y</div><div class="value" id="errorYValue">-</div></div>
          <div class="metric"><div class="label">Threshold</div><div class="value" id="thresholdValue">-</div></div>
          <div class="metric"><div class="label">Roll</div><div class="value" id="rollValue">-</div></div>
          <div class="metric"><div class="label">Pitch</div><div class="value" id="pitchValue">-</div></div>
          <div class="metric"><div class="label">Raw Roll</div><div class="value" id="rawRollValue">-</div></div>
          <div class="metric"><div class="label">Raw Pitch</div><div class="value" id="rawPitchValue">-</div></div>
          <div class="metric"><div class="label">Position Test</div><div class="value" id="positionTestMuxValue">-</div></div>
          <div class="metric"><div class="label">Individual Compensation</div><div class="value" id="muxCompValue">-</div></div>
        </div>
      </section>
      <section class="panel">
        <div class="vizOverlay">
          <canvas id="mechanismCanvas" class="pipCanvas" width="640" height="640"></canvas>
        </div>
        <div class="modes">
          <button class="alt" data-mode="OFF">OFF</button>
          <button class="alt" data-mode="ON">ON</button>
        </div>
        <details class="drawer">
          <summary>Tests and calibration</summary>
          <div class="drawerBody">
            <div class="modes drawerGroup">
              <button class="alt" data-mode="TEST">TEST</button>
              <button class="alt" data-mode="POSITION_TEST">POSITION TEST</button>
              <button class="alt" data-mode="INDIVIDUAL_TEST">INDIVIDUAL TEST</button>
              <button class="alt" data-mode="ASSEMBLY">ASSEMBLY</button>
              <button class="alt" data-mode="CALIBRATION">CALIBRATION</button>
            </div>
            <div class="actions drawerGroup">
              <button class="alt" data-test-servo="0">Servo A</button>
              <button class="alt" data-test-servo="1">Servo B</button>
              <button class="alt" data-test-servo="2">Servo C</button>
            </div>
            <div class="actions drawerGroup">
              <button data-action="compensateMux">Calibrate Individual Mux</button>
            </div>
          </div>
        </details>
        <details class="drawer">
          <summary>PID Autotune</summary>
          <div class="drawerBody">
            <div class="runtimeActions">
              <button id="pidAutotuneStart" data-action="startPidAutotune">Start Autotune</button>
              <button class="alt" id="pidAutotuneCancel" data-action="cancelPidAutotune">Cancel</button>
              <button class="alt" id="pidAutotuneRestore" data-action="restorePreviousPid">Apply Previous Gains</button>
            </div>
            <div class="banner ready" id="pidAutotuneBanner">Idle</div>
            <div class="autotuneGrid">
              <div class="autotuneMetric"><div class="label">Progress</div><div class="value" id="pidAutotuneProgressValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Candidate</div><div class="value" id="pidAutotuneCandidateValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Trial</div><div class="value" id="pidAutotuneTrialValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Captures</div><div class="value" id="pidAutotuneCapturesValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Misses</div><div class="value" id="pidAutotuneMissesValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Baseline Score</div><div class="value" id="pidAutotuneBaselineScoreValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Best Score</div><div class="value" id="pidAutotuneScoreValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Best P</div><div class="value" id="pidAutotuneBestPValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Best I</div><div class="value" id="pidAutotuneBestIValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Best D</div><div class="value" id="pidAutotuneBestDValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Entry Speed</div><div class="value" id="pidAutotuneEntrySpeedValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Capture Time</div><div class="value" id="pidAutotuneCaptureTimeValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Average Radius</div><div class="value" id="pidAutotuneAverageRadiusValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Next Candidate</div><div class="value" id="pidAutotuneNextCandidateValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Generation</div><div class="value" id="pidAutotuneGenerationValue">-</div></div>
              <div class="autotuneMetric"><div class="label">Step Size (σ)</div><div class="value" id="pidAutotuneSigmaValue">-</div></div>
            </div>
            <div class="detail" id="pidAutotuneFailureValue"></div>
          </div>
        </details>
        <details class="drawer" open>
          <summary>Configuration and runtime</summary>
          <div class="drawerBody">
            <div class="runtimeActions">
              <button data-action="saveCalibration">Save Calibration</button>
              <button data-action="resetDefaults">Reset Defaults</button>
              <button data-action="saveRuntime">Save Runtime</button>
              <button data-action="loadRuntime">Load Runtime</button>
            </div>
            <div class="controls">
              <div class="field"><label for="pGain">P Gain</label><input id="pGain" type="number" step="0.01"></div>
              <div class="field"><label for="iGain">I Gain</label><input id="iGain" type="number" step="0.01"></div>
              <div class="field"><label for="dGain">D Gain</label><input id="dGain" type="number" step="0.01"></div>
              <div class="field"><label for="ballThreshold">Ball Threshold</label><input id="ballThreshold" type="number" step="1"></div>
              <div class="field"><label for="setpointX">Setpoint X</label><input id="setpointX" type="number" step="0.01"></div>
              <div class="field"><label for="setpointY">Setpoint Y</label><input id="setpointY" type="number" step="0.01"></div>
              <div class="field"><label for="digipotTap">Digipot Tap</label><input id="digipotTap" type="number" step="1"></div>
            </div>
          </div>
        </details>
        <details class="drawer">
          <summary>System status</summary>
          <div class="drawerBody">
            <div class="banner ready" id="systemBanner">Startup state pending...</div>
            <div class="systemSummary" id="systemSummary">Waiting for system health telemetry.</div>
            <div class="healthList" id="subsystemList"></div>
          </div>
        </details>
        <div class="status" id="status">Connecting...</div>
      </section>
    </div>
  </div>
  <script>
    const fields = ['pGain','iGain','dGain','ballThreshold','setpointX','setpointY','digipotTap'];
    const fieldMap = {
      pGain:'pGain', iGain:'iGain', dGain:'dGain', ballThreshold:'ballThreshold', setpointX:'setpointX', setpointY:'setpointY',
      digipotTap:'digipotTap'
    };
    const statusEl = document.getElementById('status');
    const systemBannerEl = document.getElementById('systemBanner');
    const systemSummaryEl = document.getElementById('systemSummary');
    const subsystemListEl = document.getElementById('subsystemList');
    const canvas = document.getElementById('ballCanvas');
    const ctx = canvas.getContext('2d');
    const mechanismCanvas = document.getElementById('mechanismCanvas');
    const mechanismCtx = mechanismCanvas.getContext('2d');
    const modeButtons = Array.from(document.querySelectorAll('[data-mode]'));
    const testServoButtons = Array.from(document.querySelectorAll('[data-test-servo]'));
    let socket = null;
    let latestState = null;
    const mech = {
      servoLabels: ['A', 'B', 'C'],
      colors: ['#ff8a5b', '#58d68d', '#5dade2'],
      plateRadius: 72,
      barWidth: 18,
      barHeight: 118
    };

    function setStatus(text) { statusEl.textContent = text; }
    function fmt(value, digits = 2) { return Number.isFinite(value) ? Number(value).toFixed(digits) : '-'; }
    function escapeHtml(text) {
      return String(text || '').replace(/[&<>\"']/g, (ch) => ({ '&':'&amp;', '<':'&lt;', '>':'&gt;', '\"':'&quot;', '\'':'&#39;' }[ch] || ch));
    }
    function subsystemLabel(name) {
      const labels = {
        storage: 'Storage',
        button: 'Button',
        wifiAp: 'Wi-Fi AP',
        webUi: 'Web UI',
        actuator: 'Actuator',
        digipot: 'Digipot',
        irSensors: 'IR sensors'
      };
      return labels[name] || name;
    }
    function stateClass(state) {
      if (state === 'READY') return 'ok';
      if (state === 'DISABLED') return 'off';
      if (state === 'DEGRADED' || state === 'UNVERIFIED' || state === 'BOOTING') return 'warn';
      return 'fail';
    }
    function autotuneClass(status) {
      if (status === 'Complete') return 'ready';
      if (status === 'Failed') return 'fail';
      if (status === 'Idle') return 'ready';
      return 'warn';
    }

    function maybeSetInput(id, value) {
      const input = document.getElementById(id);
      if (document.activeElement === input) return;
      if (input.type === 'checkbox') {
        input.checked = !!value;
        return;
      }
      input.value = value;
    }
    function clamp(value, min = 0, max = 1) {
      return Math.min(max, Math.max(min, Number.isFinite(value) ? value : min));
    }
    function sectorColor(intensity) {
      const t = clamp(intensity);
      const hue = 214 - 212 * t;
      const light = 38 + 18 * t;
      return 'hsl(' + hue.toFixed(0) + ', 88%, ' + light.toFixed(0) + '%)';
    }

    function drawSensorGrid(state) {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      const pad = 54;
      const size = Math.min(canvas.width, canvas.height) - pad * 2;
      const left = (canvas.width - size) / 2;
      const top = (canvas.height - size) / 2;
      const cell = size / 4;
      const gap = 6;
      const telemetry = state && state.telemetry ? state.telemetry : {};
      const signals = Array.isArray(telemetry.sensorSignals) ? telemetry.sensorSignals.slice(0, 16).map(Number) : [];
      while (signals.length < 16) signals.push(0);
      const signalMin = Math.min(...signals);
      const signalMax = Math.max(...signals);
      const signalRange = Math.max(0, signalMax - signalMin);
      const threshold = state && state.config ? Number(state.config.ballThreshold) : 1;
      const detectionSpan = Math.max(1, signalRange, Number.isFinite(threshold) ? threshold : 1);
      const limits = state && state.limits ? state.limits : { centerXMin: -1.5, centerXMax: 1.5, centerYMin: -1.5, centerYMax: 1.5 };
      const spanX = Math.max(0.001, limits.centerXMax - limits.centerXMin);
      const spanY = Math.max(0.001, limits.centerYMax - limits.centerYMin);
      const detected = !!telemetry.ballDetected;
      const bg = ctx.createLinearGradient(0, 0, 0, canvas.height);
      bg.addColorStop(0, '#13202d');
      bg.addColorStop(1, '#080d14');
      ctx.fillStyle = bg;
      ctx.fillRect(0, 0, canvas.width, canvas.height);

      ctx.fillStyle = 'rgba(237,242,247,0.9)';
      ctx.font = '18px Verdana';
      ctx.fillText('16-sector analog mux map', left, top - 18);
      ctx.fillStyle = detected ? '#ffc3c3' : '#94a3b8';
      ctx.font = '13px Verdana';
      ctx.fillText(detected ? 'Ball detected: sectors fade blue to red by ball shadow' : 'No ball detected: sectors idle blue', left, top + size + 32);

      ctx.fillStyle = 'rgba(255,255,255,0.05)';
      ctx.fillRect(left - gap, top - gap, size + gap * 2, size + gap * 2);

      for (let row = 0; row < 4; row++) {
        for (let col = 0; col < 4; col++) {
          const index = row * 4 + col;
          const x = left + col * cell;
          const y = top + row * cell;
          const raw = Number.isFinite(signals[index]) ? signals[index] : 0;
          const intensity = detected ? clamp((signalMax - raw) / detectionSpan) : 0;
          const fill = ctx.createLinearGradient(x, y, x + cell, y + cell);
          fill.addColorStop(0, sectorColor(clamp(intensity + 0.08)));
          fill.addColorStop(1, sectorColor(clamp(intensity - 0.08)));
          ctx.fillStyle = fill;
          ctx.fillRect(x + gap / 2, y + gap / 2, cell - gap, cell - gap);
          ctx.strokeStyle = 'rgba(255,255,255,0.18)';
          ctx.lineWidth = 1;
          ctx.strokeRect(x + gap / 2, y + gap / 2, cell - gap, cell - gap);
          ctx.fillStyle = intensity > 0.62 ? 'rgba(15,23,32,0.82)' : 'rgba(237,242,247,0.78)';
          ctx.font = '12px Verdana';
          ctx.fillText('MUX ' + index, x + 14, y + 24);
          ctx.font = '16px Verdana';
          ctx.fillText(fmt(raw, 0), x + 14, y + cell - 18);
        }
      }

      ctx.strokeStyle = '#4ecdc4';
      ctx.lineWidth = 4;
      ctx.strokeRect(left, top, size, size);
      ctx.strokeStyle = 'rgba(255,255,255,0.12)';
      ctx.lineWidth = 1;
      ctx.beginPath();
      for (let i = 1; i < 4; i++) {
        ctx.moveTo(left + cell * i, top);
        ctx.lineTo(left + cell * i, top + size);
        ctx.moveTo(left, top + cell * i);
        ctx.lineTo(left + size, top + cell * i);
      }
      ctx.stroke();
      if (!detected) {
        return;
      }
      const x = (telemetry.centerX - limits.centerXMin) / spanX;
      const y = (telemetry.centerY - limits.centerYMin) / spanY;
      const ballX = left + size * clamp(x);
      const ballY = top + size * clamp(y);
      ctx.fillStyle = 'rgba(255,255,255,0.16)';
      ctx.beginPath();
      ctx.arc(ballX, ballY, 22, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = '#ffffff';
      ctx.lineWidth = 3;
      ctx.stroke();
    }

    function drawMechanismPip(state) {
      const width = mechanismCanvas.width;
      const height = mechanismCanvas.height;
      const baseWidth = 320;
      const baseHeight = 320;
      const scale = Math.min(width / baseWidth, height / baseHeight);
      mechanismCtx.clearRect(0, 0, width, height);

      const bg = mechanismCtx.createLinearGradient(0, 0, 0, height);
      bg.addColorStop(0, '#101925');
      bg.addColorStop(1, '#060a10');
      mechanismCtx.fillStyle = bg;
      mechanismCtx.fillRect(0, 0, width, height);
      mechanismCtx.save();
      mechanismCtx.scale(scale, scale);

      const telemetry = state && state.telemetry ? state.telemetry : {};
      const rollDeg = Number.isFinite(telemetry.smoothRoll) ? telemetry.smoothRoll : 0;
      const pitchDeg = Number.isFinite(telemetry.smoothPitch) ? telemetry.smoothPitch : 0;
      const rollNorm = clamp((rollDeg + 15) / 30, 0, 1);
      const pitchNorm = clamp((pitchDeg + 15) / 30, 0, 1);
      const barNorm = [
        clamp(0.5 + (pitchDeg - rollDeg) / 40, 0, 1),
        clamp(0.5 + (pitchDeg + rollDeg) / 40, 0, 1),
        clamp(0.5 + (-pitchDeg + rollDeg) / 40, 0, 1)
      ];

      mechanismCtx.fillStyle = 'rgba(237,242,247,0.92)';
      mechanismCtx.font = '16px Verdana';
      mechanismCtx.fillText('Mechanism', 18, 26);
      mechanismCtx.fillStyle = 'rgba(148,163,184,0.95)';
      mechanismCtx.font = '12px Verdana';
      mechanismCtx.fillText('Live plate + 3 actuator bars', 18, 44);

      const plateCx = 160;
      const plateCy = 128;
      const plateShiftX = (pitchNorm - 0.5) * 42;
      const plateShiftY = (rollNorm - 0.5) * 42;
      const plateRadiusX = mech.plateRadius + Math.abs(pitchDeg) * 0.8;
      const plateRadiusY = mech.plateRadius - Math.min(16, Math.abs(rollDeg) * 0.6);

      mechanismCtx.beginPath();
      mechanismCtx.ellipse(plateCx + plateShiftX, plateCy + plateShiftY, plateRadiusX, Math.max(42, plateRadiusY), 0, 0, Math.PI * 2);
      mechanismCtx.fillStyle = 'rgba(78,205,196,0.20)';
      mechanismCtx.fill();
      mechanismCtx.strokeStyle = '#4ecdc4';
      mechanismCtx.lineWidth = 4;
      mechanismCtx.stroke();

      mechanismCtx.beginPath();
      mechanismCtx.moveTo(plateCx - plateRadiusX * 0.7, plateCy + plateShiftY);
      mechanismCtx.lineTo(plateCx + plateRadiusX * 0.7, plateCy + plateShiftY);
      mechanismCtx.strokeStyle = 'rgba(255,255,255,0.18)';
      mechanismCtx.lineWidth = 2;
      mechanismCtx.stroke();

      mechanismCtx.beginPath();
      mechanismCtx.moveTo(plateCx + plateShiftX, plateCy - Math.max(42, plateRadiusY) * 0.75);
      mechanismCtx.lineTo(plateCx + plateShiftX, plateCy + Math.max(42, plateRadiusY) * 0.75);
      mechanismCtx.stroke();

      const barBaseY = 276;
      const barLeft = 58;
      const barGap = 76;
      for (let i = 0; i < 3; i++) {
        const barX = barLeft + i * barGap;
        const activeHeight = 34 + barNorm[i] * mech.barHeight;
        mechanismCtx.fillStyle = 'rgba(255,255,255,0.08)';
        mechanismCtx.fillRect(barX, barBaseY - mech.barHeight, mech.barWidth, mech.barHeight);
        mechanismCtx.fillStyle = mech.colors[i];
        mechanismCtx.fillRect(barX, barBaseY - activeHeight, mech.barWidth, activeHeight);
        mechanismCtx.strokeStyle = 'rgba(255,255,255,0.20)';
        mechanismCtx.lineWidth = 1;
        mechanismCtx.strokeRect(barX, barBaseY - mech.barHeight, mech.barWidth, mech.barHeight);
        mechanismCtx.fillStyle = '#edf2f7';
        mechanismCtx.font = '12px Verdana';
        mechanismCtx.fillText(mech.servoLabels[i], barX + 4, barBaseY + 18);
      }

      mechanismCtx.fillStyle = 'rgba(237,242,247,0.92)';
      mechanismCtx.font = '12px Verdana';
      mechanismCtx.fillText('Roll ' + fmt(rollDeg) + '°', 18, baseHeight - 34);
      mechanismCtx.fillText('Pitch ' + fmt(pitchDeg) + '°', 18, baseHeight - 16);
      mechanismCtx.restore();
    }

    function updateModeButtons(system) {
      const subsystems = system && system.subsystems ? system.subsystems : {};
      const actuatorState = subsystems.actuator && subsystems.actuator.state ? subsystems.actuator.state : 'BOOTING';
      modeButtons.forEach((button) => {
        const requiresActuator = button.dataset.mode === 'ASSEMBLY' || button.dataset.mode === 'CALIBRATION' || button.dataset.mode === 'TEST' || button.dataset.mode === 'INDIVIDUAL_TEST' || button.dataset.mode === 'POSITION_TEST';
        button.disabled = requiresActuator && actuatorState !== 'READY';
        button.title = button.disabled ? 'Servo output unavailable in current startup state' : '';
      });
    }

    function renderSystem(system) {
      if (!system) return;
      const subsystems = system.subsystems || {};
      const wifiState = subsystems.wifiAp && subsystems.wifiAp.state ? subsystems.wifiAp.state : '';
      const webState = subsystems.webUi && subsystems.webUi.state ? subsystems.webUi.state : '';
      const degraded = !!system.degradedModeActive;
      const alert = system.latestAlert || '';
      const bannerClass = (wifiState === 'FAILED' || webState === 'FAILED') ? 'fail' : degraded ? 'warn' : 'ready';
      systemBannerEl.className = 'banner ' + bannerClass;
      systemBannerEl.textContent = degraded
        ? (alert || 'System running in degraded bench mode.')
        : 'System ready for normal operation.';
      systemSummaryEl.textContent = 'Policy ' + (system.policy || 'AUTO') +
        ' | Phase ' + (system.phase || 'UNKNOWN') +
        (alert ? ' | ' + alert : '');
      subsystemListEl.innerHTML = Object.keys(subsystems).map((name) => {
        const entry = subsystems[name] || {};
        const state = entry.state || 'UNKNOWN';
        return '<div class=\"healthItem\">' +
          '<div class=\"healthHead\"><span>' + escapeHtml(subsystemLabel(name)) + '</span>' +
          '<span class=\"pill ' + stateClass(state) + '\">' + escapeHtml(state) + '</span></div>' +
          '<div class=\"detail\">' + escapeHtml(entry.detail || '') + '</div></div>';
      }).join('');
      updateModeButtons(system);
    }
    function renderPidAutotune(telemetry) {
      const status = telemetry.pidAutotuneStatus || 'Idle';
      const banner = document.getElementById('pidAutotuneBanner');
      const progress = Number(telemetry.pidAutotuneProgress || 0);
      banner.className = 'banner ' + autotuneClass(status);
      banner.textContent = status;
      document.getElementById('pidAutotuneProgressValue').textContent = progress + '%';
      document.getElementById('pidAutotuneCandidateValue').textContent = String(Number(telemetry.pidAutotuneCandidate || 0));
      document.getElementById('pidAutotuneTrialValue').textContent = String(Number(telemetry.pidAutotuneTrial || 0));
      document.getElementById('pidAutotuneCapturesValue').textContent = String(Number(telemetry.pidAutotuneCaptures || 0));
      document.getElementById('pidAutotuneMissesValue').textContent = String(Number(telemetry.pidAutotuneMisses || 0));
      document.getElementById('pidAutotuneBaselineScoreValue').textContent = Number(telemetry.pidAutotuneBaselineScore) > 0 ? fmt(Number(telemetry.pidAutotuneBaselineScore), 2) : '-';
      const hasBest = !!telemetry.pidAutotuneHasBest;
      document.getElementById('pidAutotuneScoreValue').textContent = hasBest ? fmt(Number(telemetry.pidAutotuneScore), 2) : '-';
      document.getElementById('pidAutotuneBestPValue').textContent = hasBest ? fmt(Number(telemetry.pidAutotuneBestP), 3) : '-';
      document.getElementById('pidAutotuneBestIValue').textContent = hasBest ? fmt(Number(telemetry.pidAutotuneBestI), 3) : '-';
      document.getElementById('pidAutotuneBestDValue').textContent = hasBest ? fmt(Number(telemetry.pidAutotuneBestD), 3) : '-';
      document.getElementById('pidAutotuneEntrySpeedValue').textContent = fmt(Number(telemetry.pidAutotuneEntrySpeed), 3);
      document.getElementById('pidAutotuneCaptureTimeValue').textContent = fmt(Number(telemetry.pidAutotuneCaptureTimeMs), 0) + ' ms';
      document.getElementById('pidAutotuneAverageRadiusValue').textContent = fmt(Number(telemetry.pidAutotuneAverageRadius), 3);
      document.getElementById('pidAutotuneNextCandidateValue').textContent =
        fmt(Number(telemetry.pidAutotuneNextP), 2) + ' / ' +
        fmt(Number(telemetry.pidAutotuneNextI), 3) + ' / ' +
        fmt(Number(telemetry.pidAutotuneNextD), 2);
      document.getElementById('pidAutotuneGenerationValue').textContent = String(Number(telemetry.pidAutotuneGeneration || 0));
      document.getElementById('pidAutotuneSigmaValue').textContent = fmt(Number(telemetry.pidAutotuneSigma), 3);
      document.getElementById('pidAutotuneFailureValue').textContent = telemetry.pidAutotuneLastFailure || '';
      
      const running = status !== 'Idle' && status !== 'Complete' && status !== 'Failed' && status !== 'Applying best gains';
      document.getElementById('pidAutotuneStart').disabled = running;
      document.getElementById('pidAutotuneCancel').disabled = !running;
    }

    function render(state) {
      latestState = state;
      document.getElementById('modeValue').textContent = state.telemetry.mode;
      document.getElementById('ballValue').textContent = state.telemetry.ballDetected ? 'Detected' : 'Missing';
      document.getElementById('digipotValue').textContent = state.telemetry.digipotTap;
      document.getElementById('centerXValue').textContent = fmt(state.telemetry.centerX);
      document.getElementById('centerYValue').textContent = fmt(state.telemetry.centerY);
      document.getElementById('errorXValue').textContent = fmt(state.telemetry.errorX);
      document.getElementById('errorYValue').textContent = fmt(state.telemetry.errorY);
      document.getElementById('thresholdValue').textContent = fmt(state.config.ballThreshold, 0);
      document.getElementById('rollValue').textContent = fmt(state.telemetry.smoothRoll);
      document.getElementById('pitchValue').textContent = fmt(state.telemetry.smoothPitch);
      document.getElementById('rawRollValue').textContent = fmt(state.telemetry.rawPlateRoll);
      document.getElementById('rawPitchValue').textContent = fmt(state.telemetry.rawPlatePitch);
      document.getElementById('positionTestMuxValue').textContent = state.telemetry.mode === 'POSITION_TEST' ? 'Circle' : '-';
      document.getElementById('muxCompValue').textContent = state.telemetry.muxCompensationReady ? 'Ready' : 'Not set';
      modeButtons.forEach((button) => button.classList.toggle('active', button.dataset.mode === state.telemetry.mode));
      testServoButtons.forEach((button) => button.classList.toggle('active', Number(button.dataset.testServo) === Number(state.telemetry.selectedTestServo || 0)));
      fields.forEach((id) => maybeSetInput(id, state.config[id]));
      renderPidAutotune(state.telemetry);
      renderSystem(state.system);
      drawSensorGrid(state);
      drawMechanismPip(state);
    }

    function sendJson(payload) {
      if (!socket || socket.readyState !== WebSocket.OPEN) {
        setStatus('WebSocket not connected');
        return;
      }
      socket.send(JSON.stringify(payload));
    }

    function attachInputHandlers() {
      fields.forEach((id) => {
        const input = document.getElementById(id);
        input.addEventListener('change', () => {
          const value = input.type === 'checkbox' ? (input.checked ? 1 : 0) : Number(input.value);
          const patch = {};
          patch[fieldMap[id]] = value;
          sendJson({ type: 'patch', values: patch });
        });
      });
      document.querySelectorAll('[data-mode]').forEach((button) => {
        button.addEventListener('click', () => sendJson({ type: 'action', name: 'setMode', mode: button.dataset.mode }));
      });
      document.querySelectorAll('[data-action]').forEach((button) => {
        button.addEventListener('click', () => sendJson({ type: 'action', name: button.dataset.action }));
      });
      document.querySelectorAll('[data-test-servo]').forEach((button) => {
        button.addEventListener('click', () => sendJson({ type: 'action', name: 'setTestServo', servo: Number(button.dataset.testServo) }));
      });
    }

    function connectSocket() {
      socket = new WebSocket('ws://' + window.location.host + '/ws');
      socket.onopen = () => setStatus('Connected');
      socket.onclose = () => { setStatus('Disconnected, retrying...'); setTimeout(connectSocket, 1500); };
      socket.onerror = () => setStatus('WebSocket error');
      socket.onmessage = (event) => {
        const message = JSON.parse(event.data);
        if (message.type === 'state') {
          render(message);
        } else if (message.type === 'ack') {
          setStatus(message.message);
        } else if (message.type === 'error') {
          setStatus(message.message);
        }
      };
    }

    async function loadInitialState() {
      try {
        const response = await fetch('/api/state');
        const state = await response.json();
        render(state);
      } catch (error) {
        setStatus('Initial state fetch failed');
      }
    }

    attachInputHandlers();
    loadInitialState();
    connectSocket();
  </script>
</body>
</html>
)HTML";

String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);

  for (size_t i = 0; i < value.length(); ++i) {
    const char ch = value.charAt(i);
    if (ch == '\\' || ch == '"') {
      escaped += '\\';
      escaped += ch;
    } else if (ch == '\n') {
      escaped += "\\n";
    } else if (ch == '\r') {
      escaped += "\\r";
    } else if (ch == '\t') {
      escaped += "\\t";
    } else {
      escaped += ch;
    }
  }

  return escaped;
}

void appendJsonNumber(String &json, const char *key, float value, uint8_t decimals = 4) {
  json += '"';
  json += key;
  json += "\":";
  json += String(static_cast<double>(value), static_cast<unsigned int>(decimals));
}

void appendJsonInt(String &json, const char *key, int value) {
  json += '"';
  json += key;
  json += "\":";
  json += String(value);
}

void appendJsonBool(String &json, const char *key, bool value) {
  json += '"';
  json += key;
  json += "\":";
  json += value ? "true" : "false";
}

void appendJsonString(String &json, const char *key, const String &value) {
  json += '"';
  json += key;
  json += "\":\"";
  json += jsonEscape(value);
  json += '"';
}

void appendSubsystemJson(String &json, const char *key, const SubsystemStatus &status) {
  json += '"';
  json += key;
  json += "\":{";
  appendJsonString(json, "state", String(subsystemStateToString(status.state)));
  json += ',';
  appendJsonString(json, "detail", String(status.detail));
  json += '}';
}

void appendJsonFloatArray(String &json, const char *key, const float *values, size_t count, uint8_t decimals = 2) {
  json += '"';
  json += key;
  json += "\":[";
  for (size_t i = 0; i < count; ++i) {
    if (i > 0) {
      json += ',';
    }
    json += String(static_cast<double>(values[i]), static_cast<unsigned int>(decimals));
  }
  json += ']';
}

String buildStatePayload(const TelemetryState &telemetry,
                         const RuntimeConfig &config,
                         const SystemStatus &system) {
  String payload;
  payload.reserve(3200);
  payload += "{\"type\":\"state\",\"telemetry\":{";
  appendJsonString(payload, "mode", String(modeToString(telemetry.mode)));
  payload += ',';
  appendJsonBool(payload, "ballDetected", telemetry.ballDetected);
  payload += ',';
  appendJsonNumber(payload, "centerX", telemetry.centerX);
  payload += ',';
  appendJsonNumber(payload, "centerY", telemetry.centerY);
  payload += ',';
  appendJsonNumber(payload, "errorX", telemetry.errorX);
  payload += ',';
  appendJsonNumber(payload, "errorY", telemetry.errorY);
  payload += ',';
  appendJsonNumber(payload, "plateRoll", telemetry.plateRoll);
  payload += ',';
  appendJsonNumber(payload, "platePitch", telemetry.platePitch);
  payload += ',';
  appendJsonNumber(payload, "rawPlateRoll", telemetry.rawPlateRoll);
  payload += ',';
  appendJsonNumber(payload, "rawPlatePitch", telemetry.rawPlatePitch);
  payload += ',';
  appendJsonNumber(payload, "smoothRoll", telemetry.smoothRoll);
  payload += ',';
  appendJsonNumber(payload, "smoothPitch", telemetry.smoothPitch);
  payload += ',';
  appendJsonInt(payload, "digipotTap", telemetry.digipotTap);
  payload += ',';
  appendJsonBool(payload, "muxCompensationReady", telemetry.muxCompensationReady);
  payload += ',';
  appendJsonInt(payload, "selectedTestServo", telemetry.selectedTestServo);
  payload += ',';
  appendJsonInt(payload, "positionTestMux", telemetry.positionTestMux);
  payload += ',';
  appendJsonString(payload, "pidAutotuneStatus", String(pidAutotuneStatusToString(telemetry.pidAutotuneStatus)));
  payload += ',';
  appendJsonInt(payload, "pidAutotuneProgress", telemetry.pidAutotuneProgress);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneBestP", telemetry.pidAutotuneBestP);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneBestI", telemetry.pidAutotuneBestI);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneBestD", telemetry.pidAutotuneBestD);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneScore", telemetry.pidAutotuneScore);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneOvershoot", telemetry.pidAutotuneOvershoot);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneSettlingTimeMs", telemetry.pidAutotuneSettlingTimeMs);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneAverageError", telemetry.pidAutotuneAverageError);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneBaselineScore", telemetry.pidAutotuneBaselineScore);
  payload += ',';
  appendJsonBool(payload, "pidAutotuneHasBest", telemetry.pidAutotuneHasBest);
  payload += ',';
  appendJsonInt(payload, "pidAutotuneCandidate", telemetry.pidAutotuneCandidate);
  payload += ',';
  appendJsonInt(payload, "pidAutotuneTrial", telemetry.pidAutotuneTrial);
  payload += ',';
  appendJsonInt(payload, "pidAutotuneCaptures", telemetry.pidAutotuneCaptures);
  payload += ',';
  appendJsonInt(payload, "pidAutotuneMisses", telemetry.pidAutotuneMisses);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneEntrySpeed", telemetry.pidAutotuneEntrySpeed);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneCaptureTimeMs", telemetry.pidAutotuneCaptureTimeMs);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneAverageRadius", telemetry.pidAutotuneAverageRadius);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneNextP", telemetry.pidAutotuneNextP);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneNextI", telemetry.pidAutotuneNextI);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneNextD", telemetry.pidAutotuneNextD);
  payload += ',';
  appendJsonInt(payload, "pidAutotuneGeneration", telemetry.pidAutotuneGeneration);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneSigma", telemetry.pidAutotuneSigma);
  payload += ',';
  appendJsonNumber(payload, "pidAutotuneNoiseFloor", 0.0f);
  payload += ',';
  appendJsonString(payload, "pidAutotuneLastFailure", String(telemetry.pidAutotuneLastFailure));
  payload += ',';
  appendJsonFloatArray(payload, "sensorSignals", telemetry.sensorSignals, kSensorCount);
  payload += "},\"config\":{";
  appendJsonNumber(payload, "pGain", config.pGain);
  payload += ',';
  appendJsonNumber(payload, "iGain", config.iGain);
  payload += ',';
  appendJsonNumber(payload, "dGain", config.dGain);
  payload += ',';
  appendJsonNumber(payload, "setpointX", config.setpointX);
  payload += ',';
  appendJsonNumber(payload, "setpointY", config.setpointY);
  payload += ',';
  appendJsonNumber(payload, "ballThreshold", config.ballThreshold);
  payload += ',';
  appendJsonInt(payload, "digipotTap", config.digipotTap);
  payload += "},\"limits\":{";
  appendJsonNumber(payload, "centerXMin", kCenterXMin);
  payload += ',';
  appendJsonNumber(payload, "centerXMax", kCenterXMax);
  payload += ',';
  appendJsonNumber(payload, "centerYMin", kCenterYMin);
  payload += ',';
  appendJsonNumber(payload, "centerYMax", kCenterYMax);
  payload += "},\"system\":{";
  appendJsonString(payload, "policy", String(startupPolicyToString(system.policy)));
  payload += ',';
  appendJsonString(payload, "phase", String(startupPhaseToString(system.phase)));
  payload += ',';
  appendJsonBool(payload, "degradedModeActive", system.degradedModeActive);
  payload += ',';
  appendJsonString(payload, "latestAlert", String(system.latestAlert));
  payload += ",\"subsystems\":{";
  appendSubsystemJson(payload, "storage", system.storage);
  payload += ',';
  appendSubsystemJson(payload, "button", system.button);
  payload += ',';
  appendSubsystemJson(payload, "wifiAp", system.wifiAp);
  payload += ',';
  appendSubsystemJson(payload, "webUi", system.webUi);
  payload += ',';
  appendSubsystemJson(payload, "actuator", system.actuator);
  payload += ',';
  appendSubsystemJson(payload, "digipot", system.digipot);
  payload += ',';
  appendSubsystemJson(payload, "irSensors", system.irSensors);
  payload += "}}}";
  return payload;
}

String buildMessagePayload(const char *type, const char *message, bool includeOk = false) {
  String payload;
  payload.reserve(128);
  payload += "{\"type\":\"";
  payload += type;
  payload += '"';
  if (includeOk) {
    payload += ",\"ok\":true";
  }
  payload += ",\"message\":\"";
  payload += jsonEscape(String(message != nullptr ? message : ""));
  payload += "\"}";
  return payload;
}

bool findKeyPosition(const String &json, const char *key, int &valueStart) {
  const String pattern = String("\"") + key + '"';
  const int keyPos = json.indexOf(pattern);
  if (keyPos < 0) {
    return false;
  }

  const int colonPos = json.indexOf(':', keyPos + pattern.length());
  if (colonPos < 0) {
    return false;
  }

  valueStart = colonPos + 1;
  while (valueStart < static_cast<int>(json.length()) &&
         (json.charAt(valueStart) == ' ' || json.charAt(valueStart) == '\n' ||
          json.charAt(valueStart) == '\r' || json.charAt(valueStart) == '\t')) {
    ++valueStart;
  }

  return valueStart < static_cast<int>(json.length());
}

bool extractStringField(const String &json, const char *key, String &value) {
  int valueStart = 0;
  if (!findKeyPosition(json, key, valueStart) || json.charAt(valueStart) != '"') {
    return false;
  }

  ++valueStart;
  String result;
  bool escapeNext = false;
  for (int i = valueStart; i < static_cast<int>(json.length()); ++i) {
    const char ch = json.charAt(i);
    if (escapeNext) {
      result += ch;
      escapeNext = false;
      continue;
    }
    if (ch == '\\') {
      escapeNext = true;
      continue;
    }
    if (ch == '"') {
      value = result;
      return true;
    }
    result += ch;
  }

  return false;
}

bool extractNumberField(const String &json, const char *key, float &value) {
  int valueStart = 0;
  if (!findKeyPosition(json, key, valueStart)) {
    return false;
  }

  int valueEnd = valueStart;
  while (valueEnd < static_cast<int>(json.length())) {
    const char ch = json.charAt(valueEnd);
    if ((ch >= '0' && ch <= '9') || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E') {
      ++valueEnd;
      continue;
    }
    break;
  }

  if (valueEnd == valueStart) {
    return false;
  }

  value = json.substring(valueStart, valueEnd).toFloat();
  return true;
}

bool extractIntField(const String &json, const char *key, int &value) {
  float parsed = 0.0f;
  if (!extractNumberField(json, key, parsed)) {
    return false;
  }

  value = static_cast<int>(parsed);
  return true;
}

}

WebUiServer::WebUiServer(SharedState &sharedState, DebugLogger &debugLogger)
    : sharedState_(sharedState),
      debugLogger_(debugLogger),
      server_(80),
      webSocket_("/ws"),
      taskHandle_(nullptr),
      lastBroadcastMs_(0) {}

bool WebUiServer::begin() {
  debugLogger_.tryLogf("Starting Wi-Fi AP task on core %d", static_cast<int>(kWebTaskCore));
  WiFi.mode(WIFI_AP);
  bool apStarted = false;
  if (strlen(kWifiApPassword) >= 8) {
    apStarted = WiFi.softAP(kWifiApSsid, kWifiApPassword);
  } else {
    apStarted = WiFi.softAP(kWifiApSsid);
    debugLogger_.tryLog("Warning: Wi-Fi AP password shorter than 8 characters, starting open AP.");
  }

  if (!apStarted) {
    debugLogger_.tryLog("Wi-Fi AP startup failed.");
    return false;
  }

  debugLogger_.tryLogf("Wi-Fi AP started. Gateway IP: %s", WiFi.softAPIP().toString().c_str());

  webSocket_.onEvent([this](AsyncWebSocket *server,
                            AsyncWebSocketClient *client,
                            AwsEventType type,
                            void *arg,
                            uint8_t *payload,
                            size_t length) {
    handleWebSocketEvent(server, client, type, arg, payload, length);
  });
  server_.addHandler(&webSocket_);
  configureRoutes();
  server_.begin();

  const BaseType_t taskCreated = xTaskCreatePinnedToCore(taskEntryPoint,
                                                         "babot-web",
                                                         kWebTaskStackSize,
                                                         this,
                                                         kWebTaskPriority,
                                                         &taskHandle_,
                                                         kWebTaskCore);
  if (taskCreated != pdPASS) {
    debugLogger_.tryLog("Failed to create Wi-Fi/web task.");
    return false;
  }

  return true;
}

void WebUiServer::taskEntryPoint(void *parameter) {
  static_cast<WebUiServer *>(parameter)->taskLoop();
}

void WebUiServer::taskLoop() {
  for (;;) {
    pollDebugOutput();
    webSocket_.cleanupClients();

    if (millis() - lastBroadcastMs_ >= kTelemetryBroadcastMs) {
      broadcastState();
      lastBroadcastMs_ = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void WebUiServer::pollDebugOutput() {
  debugLogger_.flushSome(kDebugFlushBudgetPerLoop);
}

void WebUiServer::configureRoutes() {
  server_.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { handleIndex(request); });
  server_.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest *request) { handleState(request); });
}

void WebUiServer::handleIndex(AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", kIndexHtml);
}

void WebUiServer::handleState(AsyncWebServerRequest *request) {
  TelemetryState telemetry;
  RuntimeConfig config;
  SystemStatus system;
  sharedState_.snapshot(telemetry, config);
  sharedState_.snapshotSystem(system);

  String payload = buildStatePayload(telemetry, config, system);
  request->send(200, "application/json", payload);
}

void WebUiServer::handleWebSocketEvent(AsyncWebSocket *server,
                                       AsyncWebSocketClient *client,
                                       AwsEventType type,
                                       void *arg,
                                       uint8_t *payload,
                                       size_t length) {
  (void)server;

  if (type == WS_EVT_CONNECT) {
    debugLogger_.tryLogf("WebSocket client %u connected.", static_cast<unsigned>(client->id()));
    sendStateToClient(client);
    return;
  }

  if (type != WS_EVT_DATA) {
    return;
  }

  AwsFrameInfo *frameInfo = static_cast<AwsFrameInfo *>(arg);
  if (frameInfo == nullptr ||
      frameInfo->opcode != WS_TEXT ||
      !frameInfo->final ||
      frameInfo->index != 0 ||
      frameInfo->len != length) {
    sendError(client, "Unsupported WebSocket frame");
    return;
  }

  UiCommand command{};
  String errorMessage;
  if (!parseMessage(payload, length, command, errorMessage)) {
    debugLogger_.tryLogf("Rejected WebSocket message from client %u: %s",
                         static_cast<unsigned>(client->id()),
                         errorMessage.c_str());
    sendError(client, errorMessage.c_str());
    return;
  }

  if (!sharedState_.enqueueCommand(command)) {
    debugLogger_.tryLog("UI command queue full.");
    sendError(client, "Command queue full");
    return;
  }

  sendAck(client, "Command queued");
}

void WebUiServer::broadcastState() {
  TelemetryState telemetry;
  RuntimeConfig config;
  SystemStatus system;
  sharedState_.snapshot(telemetry, config);
  sharedState_.snapshotSystem(system);

  String payload = buildStatePayload(telemetry, config, system);
  webSocket_.textAll(payload);
}

void WebUiServer::sendStateToClient(AsyncWebSocketClient *client) {
  TelemetryState telemetry;
  RuntimeConfig config;
  SystemStatus system;
  sharedState_.snapshot(telemetry, config);
  sharedState_.snapshotSystem(system);

  String payload = buildStatePayload(telemetry, config, system);
  client->text(payload);
}

void WebUiServer::sendError(AsyncWebSocketClient *client, const char *message) {
  String payload = buildMessagePayload("error", message);
  client->text(payload);
}

void WebUiServer::sendAck(AsyncWebSocketClient *client, const char *message) {
  String payload = buildMessagePayload("ack", message, true);
  client->text(payload);
}

bool WebUiServer::parseMessage(const uint8_t *payload,
                               size_t length,
                               UiCommand &command,
                               String &errorMessage) {
  const String message(reinterpret_cast<const char *>(payload), length);
  String type;
  if (!extractStringField(message, "type", type)) {
    errorMessage = "Invalid JSON";
    return false;
  }

  if (type == "patch") {
    command.type = UI_PATCH_CONFIG;
    command.fieldsMask = kFieldNone;
    command.config = defaultRuntimeConfig();

    float floatValue = 0.0f;
    int intValue = 0;

    if (extractNumberField(message, "pGain", floatValue)) {
      command.fieldsMask |= kFieldPGain;
      command.config.pGain = floatValue;
    }
    if (extractNumberField(message, "iGain", floatValue)) {
      command.fieldsMask |= kFieldIGain;
      command.config.iGain = floatValue;
    }
    if (extractNumberField(message, "dGain", floatValue)) {
      command.fieldsMask |= kFieldDGain;
      command.config.dGain = floatValue;
    }
    if (extractNumberField(message, "setpointX", floatValue)) {
      command.fieldsMask |= kFieldSetpointX;
      command.config.setpointX = floatValue;
    }
    if (extractNumberField(message, "setpointY", floatValue)) {
      command.fieldsMask |= kFieldSetpointY;
      command.config.setpointY = floatValue;
    }
    if (extractNumberField(message, "ballThreshold", floatValue)) {
      command.fieldsMask |= kFieldBallThreshold;
      command.config.ballThreshold = floatValue;
    }
    if (extractIntField(message, "digipotTap", intValue)) {
      command.fieldsMask |= kFieldDigipotTap;
      command.config.digipotTap = static_cast<uint8_t>(intValue);
    }
    if (command.fieldsMask == kFieldNone) {
      errorMessage = "No supported fields in patch";
      return false;
    }
    return true;
  }

  if (type == "action") {
    String name;
    if (!extractStringField(message, "name", name)) {
      errorMessage = "Missing action name";
      return false;
    }

    if (name == "saveRuntime") {
      command.type = UI_SAVE_RUNTIME;
      return true;
    }
    if (name == "loadRuntime") {
      command.type = UI_LOAD_RUNTIME;
      return true;
    }
    if (name == "saveCalibration") {
      command.type = UI_SAVE_CALIBRATION;
      return true;
    }
    if (name == "resetDefaults") {
      command.type = UI_RESET_DEFAULTS;
      return true;
    }
    if (name == "compensateMux") {
      command.type = UI_COMPENSATE_MUX;
      return true;
    }
    if (name == "startPidAutotune") {
      command.type = UI_START_PID_AUTOTUNE;
      return true;
    }
    if (name == "cancelPidAutotune") {
      command.type = UI_CANCEL_PID_AUTOTUNE;
      return true;
    }
    if (name == "restorePreviousPid") {
      command.type = UI_RESTORE_PREVIOUS_PID;
      return true;
    }
    if (name == "setTestServo") {
      int servo = 0;
      if (!extractIntField(message, "servo", servo) || servo < 0 || servo > 2) {
        errorMessage = "Invalid test servo";
        return false;
      }
      command.type = UI_SET_TEST_SERVO;
      command.testServo = static_cast<uint8_t>(servo);
      return true;
    }
    if (name == "setMode") {
      String modeText;
      if (!extractStringField(message, "mode", modeText)) {
        errorMessage = "Missing mode";
        return false;
      }
      BaBotMode mode = OFF;
      if (!modeFromString(modeText, mode)) {
        errorMessage = "Invalid mode";
        return false;
      }
      command.type = UI_SET_MODE;
      command.mode = mode;
      return true;
    }
    errorMessage = "Unknown action";
    return false;
  }

  errorMessage = "Unknown message type";
  return false;
}

}
