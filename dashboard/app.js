/**
 * EMG Bionic Control System — High-Speed Telemetry Client
 * Features:
 * - 60 FPS Multi-Mode Oscilloscope (Time-Domain, FFT Spectrum, Dual View)
 * - Deep Neural Network 4-Class Softmax Probability Distribution Visualizer
 * - Articulated Cybernetic Hand & Servo Dial
 * - Live Session Clock, Latency & Diagnostics Strip
 * - Web Audio API Synthetic Feedback Ticks
 * - Telemetry Snapshot CSV Exporter
 * - Keyboard Shortcuts [1, 2, 3, 4, SPACE, P, T]
 */

(() => {
    'use strict';

    // DOM Elements
    const canvas = document.getElementById('emg-canvas');
    const ctx = canvas.getContext('2d');
    const wsDot = document.getElementById('ws-dot');
    const wsStatusText = document.getElementById('ws-status-text');
    const udpDot = document.getElementById('udp-dot');
    const udpStatusText = document.getElementById('udp-status-text');
    const watchdogBadge = document.getElementById('watchdog-badge');
    const watchdogDot = document.getElementById('watchdog-dot');
    const watchdogText = document.getElementById('watchdog-text');
    const sessionClock = document.getElementById('session-clock');

    const gestureBadge = document.getElementById('gesture-badge');
    const gestureName = document.getElementById('gesture-name');
    const gestureIcon = document.getElementById('gesture-icon');
    const gestureSafePill = document.getElementById('gesture-safe-pill');
    const confPct = document.getElementById('conf-pct');
    const confFill = document.getElementById('conf-fill');
    const streamTag = document.getElementById('stream-tag');

    // Softmax elements
    const smItems = {
        RELAX: { el: document.getElementById('sm-item-relax'), val: document.getElementById('sm-val-relax'), fill: document.getElementById('sm-fill-relax') },
        GRASP: { el: document.getElementById('sm-item-grasp'), val: document.getElementById('sm-val-grasp'), fill: document.getElementById('sm-fill-grasp') },
        OPEN:  { el: document.getElementById('sm-item-open'),  val: document.getElementById('sm-val-open'),  fill: document.getElementById('sm-fill-open') },
        CLOSE: { el: document.getElementById('sm-item-close'), val: document.getElementById('sm-val-close'), fill: document.getElementById('sm-fill-close') }
    };

    const servoAngleDisplay = document.getElementById('servo-angle-display');
    const gaugeNeedle = document.getElementById('gauge-needle');
    const gaugeActiveArc = document.getElementById('gauge-active-arc');
    const bionicHand = document.getElementById('bionic-hand');
    const handStateText = document.getElementById('hand-state-text');
    const palmCore = document.getElementById('palm-core');

    const statPackets = document.getElementById('stat-packets');
    const statRate = document.getElementById('stat-rate');
    const statSeq = document.getElementById('stat-seq');
    const statCrc = document.getElementById('stat-crc');
    const statLatency = document.getElementById('stat-latency');
    const statInference = document.getElementById('stat-inference');
    const statWatchdog = document.getElementById('stat-watchdog');

    const hudPeakV = document.getElementById('hud-peak-v');
    const hudMdf = document.getElementById('hud-mdf');
    const legendSpec = document.getElementById('legend-spec');
    const btnPause = document.getElementById('btn-pause-scope');

    // Tabs
    const tabTime = document.getElementById('tab-time');
    const tabSpectrum = document.getElementById('tab-spectrum');
    const tabDual = document.getElementById('tab-dual');

    // Modals & Action buttons
    const btnOpenTrain = document.getElementById('btn-open-train');
    const modalTrain = document.getElementById('modal-train');
    const btnCloseModal = document.getElementById('btn-close-modal');
    const btnCloseModalDone = document.getElementById('btn-close-modal-done');
    const btnCopyTrainCmd = document.getElementById('btn-copy-train-cmd');
    const trainCmdText = document.getElementById('train-cmd-text');

    const btnExportCsv = document.getElementById('btn-export-csv');
    const btnToggleScanlines = document.getElementById('btn-toggle-scanlines');
    const screenScanlines = document.getElementById('screen-scanlines');
    const btnToggleAudio = document.getElementById('btn-toggle-audio');
    const audioIcon = document.getElementById('audio-icon');

    // Feature element map
    const featuresMap = {
        rms: { val: document.getElementById('feat-rms'), bar: document.getElementById('meter-rms'), max: 1.0 },
        mav: { val: document.getElementById('feat-mav'), bar: document.getElementById('meter-mav'), max: 0.8 },
        variance: { val: document.getElementById('feat-var'), bar: document.getElementById('meter-var'), max: 0.25 },
        wl: { val: document.getElementById('feat-wl'), bar: document.getElementById('meter-wl'), max: 15.0 },
        zc: { val: document.getElementById('feat-zc'), bar: document.getElementById('meter-zc'), max: 40 },
        ssc: { val: document.getElementById('feat-ssc'), bar: document.getElementById('meter-ssc'), max: 40 }
    };

    const GESTURE_CONFIG = {
        0: { name: 'NONE', icon: '⚠️', desc: 'Safe State / Motor Neutral (90°)', cls: 'gesture-NONE' },
        1: { name: 'RELAX', icon: '✋', desc: 'Resting Muscle / Neutral (90°)', cls: 'gesture-RELAX' },
        2: { name: 'GRASP', icon: '✊', desc: 'Closed Power Grip (0°)', cls: 'gesture-GRASP' },
        3: { name: 'OPEN', icon: '🖐️', desc: 'Full Hand Open (180°)', cls: 'gesture-OPEN' },
        4: { name: 'CLOSE', icon: '👊', desc: 'Precision Pinch Grip (0°)', cls: 'gesture-CLOSE' }
    };

    // State Variables
    const MAX_BUFFER = 600;
    const rawBuffer = new Float32Array(MAX_BUFFER);
    const filteredBuffer = new Float32Array(MAX_BUFFER);
    let spectrumData = new Float32Array(16);
    let bufferIndex = 0;
    let isPaused = false;
    let scopeMode = 'time'; // 'time', 'spectrum', 'dual'
    let currentAngle = 90;
    let targetAngle = 90;
    let ws = null;
    let audioEnabled = false;
    let audioCtx = null;
    let lastCmd = 1;
    let peakV = 0.0;
    const sessionStartTime = Date.now();

    // History for CSV Export
    const telemetryHistory = [];
    const MAX_CSV_HISTORY = 1000;

    // Resize canvas high DPI
    function resizeCanvas() {
        const rect = canvas.parentElement.getBoundingClientRect();
        canvas.width = rect.width * window.devicePixelRatio;
        canvas.height = rect.height * window.devicePixelRatio;
        ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
    }
    window.addEventListener('resize', resizeCanvas);
    resizeCanvas();

    // Web Audio synthesizer for gesture transition clicks
    function playBeep(freq = 440, duration = 0.04) {
        if (!audioEnabled) return;
        try {
            if (!audioCtx) audioCtx = new (window.AudioContext || window.webkitAudioContext)();
            const osc = audioCtx.createOscillator();
            const gain = audioCtx.createGain();
            osc.type = 'sine';
            osc.frequency.setValueAtTime(freq, audioCtx.currentTime);
            gain.gain.setValueAtTime(0.08, audioCtx.currentTime);
            gain.gain.exponentialRampToValueAtTime(0.001, audioCtx.currentTime + duration);
            osc.connect(gain);
            gain.connect(audioCtx.destination);
            osc.start();
            osc.stop(audioCtx.currentTime + duration);
        } catch (e) {
            // AudioContext policy
        }
    }

    // Session Clock Timer
    setInterval(() => {
        const elapsed = Math.floor((Date.now() - sessionStartTime) / 1000);
        const hrs = String(Math.floor(elapsed / 3600)).padStart(2, '0');
        const mins = String(Math.floor((elapsed % 3600) / 60)).padStart(2, '0');
        const secs = String(elapsed % 60).padStart(2, '0');
        sessionClock.textContent = `SESSION ${hrs}:${mins}:${secs}`;
    }, 1000);

    // ============================================================
    // Oscilloscope & Spectral Draw Loops (60 FPS)
    // ============================================================

    function drawTimeDomain(xStart, yStart, w, h) {
        const midY = yStart + h / 2;

        // Center zero grid line
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)';
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 4]);
        ctx.beginPath();
        ctx.moveTo(xStart, midY);
        ctx.lineTo(xStart + w, midY);
        ctx.stroke();
        ctx.setLineDash([]);

        const step = w / MAX_BUFFER;
        const scale = h * 0.42;

        // Trace 1: Raw Signal (Cyan)
        ctx.strokeStyle = '#38bdf8';
        ctx.lineWidth = 1.0;
        ctx.beginPath();
        for (let i = 0; i < MAX_BUFFER; i++) {
            const idx = (bufferIndex + i) % MAX_BUFFER;
            const x = xStart + (i * step);
            const y = midY - (rawBuffer[idx] * scale);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();

        // Trace 2: Filtered Signal (Emerald Glow)
        ctx.strokeStyle = '#34d399';
        ctx.lineWidth = 2.0;
        ctx.shadowColor = '#34d399';
        ctx.shadowBlur = 8;
        ctx.beginPath();
        for (let i = 0; i < MAX_BUFFER; i++) {
            const idx = (bufferIndex + i) % MAX_BUFFER;
            const x = xStart + (i * step);
            const y = midY - (filteredBuffer[idx] * scale);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
        ctx.shadowBlur = 0;
    }

    function drawSpectrum(xStart, yStart, w, h) {
        const numBars = spectrumData.length;
        const barWidth = (w / numBars) * 0.72;
        const barGap = (w / numBars) * 0.28;

        // Gradient for spectrum bars
        const grad = ctx.createLinearGradient(0, yStart + h, 0, yStart);
        grad.addColorStop(0, '#00f2fe');
        grad.addColorStop(0.5, '#38bdf8');
        grad.addColorStop(1, '#c084fc');

        for (let i = 0; i < numBars; i++) {
            const pwr = spectrumData[i] || 0.05;
            const barH = Math.max(4, pwr * (h - 24));
            const x = xStart + i * (barWidth + barGap) + 8;
            const y = (yStart + h) - barH - 4;

            ctx.fillStyle = grad;
            ctx.shadowColor = '#00f2fe';
            ctx.shadowBlur = pwr > 0.6 ? 10 : 0;
            ctx.fillRect(x, y, barWidth, barH);
            ctx.shadowBlur = 0;

            // Frequency tick labels
            if (i % 4 === 0) {
                const freq = Math.round(20 + i * (430 / numBars));
                ctx.fillStyle = 'rgba(255, 255, 255, 0.4)';
                ctx.font = '9px JetBrains Mono';
                ctx.fillText(`${freq}Hz`, x, yStart + h - 2);
            }
        }
    }

    function renderScreen() {
        requestAnimationFrame(renderScreen);
        if (isPaused) return;

        const w = canvas.parentElement.clientWidth;
        const h = canvas.parentElement.clientHeight;

        ctx.clearRect(0, 0, w, h);

        if (scopeMode === 'time') {
            drawTimeDomain(0, 0, w, h);
        } else if (scopeMode === 'spectrum') {
            drawSpectrum(0, 0, w, h);
        } else if (scopeMode === 'dual') {
            const splitH = h / 2;
            drawTimeDomain(0, 0, w, splitH);
            drawSpectrum(0, splitH, w, splitH);

            // Dividing line
            ctx.strokeStyle = 'rgba(56, 189, 248, 0.25)';
            ctx.lineWidth = 1;
            ctx.beginPath();
            ctx.moveTo(0, splitH);
            ctx.lineTo(w, splitH);
            ctx.stroke();
        }
    }
    requestAnimationFrame(renderScreen);

    // ============================================================
    // Smooth Servo Gauge & Articulated Bionic Hand Animator
    // ============================================================

    function updateServoGauge(angle) {
        targetAngle = angle;
        currentAngle += (targetAngle - currentAngle) * 0.22;
        const rounded = Math.round(currentAngle);
        servoAngleDisplay.textContent = rounded;

        // Needle rotation: -90° at 0, 0° at 90, +90° at 180
        const rotDeg = currentAngle - 90;
        gaugeNeedle.setAttribute('transform', `rotate(${rotDeg} 100 100)`);

        // Bionic Hand Articulation
        bionicHand.classList.remove('hand-grip-closed', 'hand-grip-open');
        if (rounded <= 25) {
            bionicHand.classList.add('hand-grip-closed');
            handStateText.textContent = `Full Power Grasp (${rounded}°)`;
        } else if (rounded >= 155) {
            bionicHand.classList.add('hand-grip-open');
            handStateText.textContent = `Extended Hand Open (${rounded}°)`;
        } else {
            handStateText.textContent = `Neutral Rest Posture (${rounded}°)`;
        }
    }

    // ============================================================
    // Telemetry Frame Processor
    // ============================================================

    function onTelemetryFrame(frame) {
        // 1. Oscilloscope Samples
        const raw = frame.raw_samples || [];
        const filt = frame.filtered_samples || [];
        let maxV = 0.0;
        for (let i = 0; i < raw.length; i++) {
            rawBuffer[bufferIndex] = raw[i];
            filteredBuffer[bufferIndex] = filt[i] || 0;
            const absF = Math.abs(filt[i] || 0);
            if (absF > maxV) maxV = absF;
            bufferIndex = (bufferIndex + 1) % MAX_BUFFER;
        }

        peakV = peakV * 0.95 + maxV * 0.05;
        hudPeakV.textContent = `±${peakV.toFixed(2)} V`;

        // 2. Frequency Spectrum & MDF
        if (frame.spectrum) {
            for (let i = 0; i < frame.spectrum.length && i < 16; i++) {
                spectrumData[i] = frame.spectrum[i];
            }
        }
        if (frame.median_freq_hz) {
            hudMdf.textContent = `${frame.median_freq_hz.toFixed(1)} Hz`;
        }

        // 3. Clinical Features
        const feat = frame.features || {};
        if (feat.rms !== undefined) {
            featuresMap.rms.val.textContent = feat.rms.toFixed(4);
            featuresMap.rms.bar.style.width = Math.min(100, (feat.rms / featuresMap.rms.max) * 100) + '%';
        }
        if (feat.mav !== undefined) {
            featuresMap.mav.val.textContent = feat.mav.toFixed(4);
            featuresMap.mav.bar.style.width = Math.min(100, (feat.mav / featuresMap.mav.max) * 100) + '%';
        }
        if (feat.variance !== undefined) {
            featuresMap.variance.val.textContent = feat.variance.toFixed(6);
            featuresMap.variance.bar.style.width = Math.min(100, (feat.variance / featuresMap.variance.max) * 100) + '%';
        }
        if (feat.wl !== undefined) {
            featuresMap.wl.val.textContent = feat.wl.toFixed(4);
            featuresMap.wl.bar.style.width = Math.min(100, (feat.wl / featuresMap.wl.max) * 100) + '%';
        }
        if (feat.zc !== undefined) {
            featuresMap.zc.val.textContent = feat.zc;
            featuresMap.zc.bar.style.width = Math.min(100, (feat.zc / featuresMap.zc.max) * 100) + '%';
        }
        if (feat.ssc !== undefined) {
            featuresMap.ssc.val.textContent = feat.ssc;
            featuresMap.ssc.bar.style.width = Math.min(100, (feat.ssc / featuresMap.ssc.max) * 100) + '%';
        }

        // 4. Gesture Classification & Audio Trigger
        const cmdId = frame.command_id || 0;
        const gConf = GESTURE_CONFIG[cmdId] || GESTURE_CONFIG[0];
        if (cmdId !== lastCmd) {
            playBeep(cmdId === 0 ? 300 : 580, 0.05);
            lastCmd = cmdId;
        }

        gestureName.textContent = gConf.name;
        gestureIcon.textContent = gConf.icon;
        gestureBadge.className = 'gesture-badge ' + gConf.cls;

        const conf = frame.confidence || 0.0;
        const pct = Math.round(conf * 100);
        confPct.textContent = pct + '%';
        confFill.style.width = pct + '%';

        if (conf >= 0.60) {
            gestureSafePill.textContent = 'GATE PASS (≥60%)';
            gestureSafePill.style.color = 'var(--accent-green)';
            gestureSafePill.style.borderColor = 'rgba(52, 211, 153, 0.4)';
        } else {
            gestureSafePill.textContent = 'CONFIDENCE FAIL (<60%)';
            gestureSafePill.style.color = 'var(--accent-red)';
            gestureSafePill.style.borderColor = 'rgba(244, 63, 94, 0.4)';
        }

        // 5. Deep Neural Network Softmax Multi-Class Probability
        const probs = frame.probabilities || {
            RELAX: cmdId === 1 ? conf : 0.05,
            GRASP: cmdId === 2 ? conf : 0.05,
            OPEN:  cmdId === 3 ? conf : 0.05,
            CLOSE: cmdId === 4 ? conf : 0.05
        };

        const activeName = gConf.name;
        for (const [key, item] of Object.entries(smItems)) {
            const p = (probs[key] !== undefined ? probs[key] : 0.0) * 100;
            item.val.textContent = p.toFixed(1) + '%';
            item.fill.style.width = Math.min(100, Math.max(1, p)) + '%';
            if (key === activeName) {
                item.el.classList.add('active-class');
            } else {
                item.el.classList.remove('active-class');
            }
        }

        // 6. Actuator Gauge & Hand
        updateServoGauge(frame.servo_angle !== undefined ? frame.servo_angle : 90);

        // 7. Telemetry & Diagnostic Metrics
        statPackets.textContent = frame.packets_received || 0;
        statRate.innerHTML = (frame.packet_rate_hz || 0).toFixed(1) + ' <small>Hz</small>';
        statSeq.textContent = frame.sequence_number || 0;

        if (frame.diagnostics) {
            statLatency.innerHTML = `${frame.diagnostics.pipeline_latency_ms} <small>ms</small>`;
            statInference.innerHTML = `${frame.diagnostics.inference_latency_us} <small>µs</small>`;
        }

        if (frame.manual_override) {
            streamTag.textContent = 'MANUAL INJECTION';
            streamTag.style.color = 'var(--accent-amber)';
        } else if (frame.packets_received > 0) {
            streamTag.textContent = 'LIVE UDP STREAM';
            streamTag.style.color = 'var(--accent-green)';
            udpDot.className = 'badge-dot dot-online';
            udpStatusText.textContent = `UDP ACTIVE (${frame.packet_rate_hz || 0} Hz)`;
        } else {
            streamTag.textContent = 'SIMULATED DEMO';
            streamTag.style.color = 'var(--accent-cyan)';
            udpDot.className = 'badge-dot dot-offline';
            udpStatusText.textContent = 'UDP: WAITING';
        }

        // 8. Watchdog state
        if (frame.watchdog_timed_out) {
            watchdogBadge.classList.add('watchdog-alert');
            watchdogDot.className = 'badge-dot dot-offline';
            watchdogText.textContent = 'WATCHDOG: TIMEOUT (SAFE 90°)';
            statWatchdog.textContent = 'TRIGGERED (SAFE)';
            statWatchdog.className = 'stat-value status-bad';
        } else {
            watchdogBadge.classList.remove('watchdog-alert');
            watchdogDot.className = 'badge-dot dot-online';
            watchdogText.textContent = 'SAFETY WATCHDOG: ARMED';
            statWatchdog.textContent = 'ACTIVE (500ms)';
            statWatchdog.className = 'stat-value status-good';
        }

        // 9. Store for CSV Export
        if (telemetryHistory.length < MAX_CSV_HISTORY) {
            telemetryHistory.push({
                timestamp: new Date().toISOString(),
                seq: frame.sequence_number || 0,
                command: gConf.name,
                confidence: conf,
                servo_angle: frame.servo_angle,
                rms: feat.rms || 0,
                mav: feat.mav || 0,
                var: feat.variance || 0,
                wl: feat.wl || 0,
                zc: feat.zc || 0,
                ssc: feat.ssc || 0
            });
        }
    }

    // ============================================================
    // WebSocket Connection
    // ============================================================

    function connectWS() {
        const host = window.location.hostname || 'localhost';
        const wsUrl = `ws://${host}:8081`;

        ws = new WebSocket(wsUrl);

        ws.onopen = () => {
            wsDot.className = 'badge-dot dot-online';
            wsStatusText.textContent = 'CONNECTED';
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                if (data.type === 'telemetry') {
                    onTelemetryFrame(data);
                }
            } catch (err) {
                console.error('Telemetry frame decode error:', err);
            }
        };

        ws.onclose = () => {
            wsDot.className = 'badge-dot dot-offline';
            wsStatusText.textContent = 'RECONNECTING...';
            setTimeout(connectWS, 1200);
        };

        ws.onerror = () => {
            ws.close();
        };
    }

    // ============================================================
    // Event Listeners & UI Controls
    // ============================================================

    // Pause button
    btnPause.addEventListener('click', () => {
        isPaused = !isPaused;
        btnPause.textContent = isPaused ? 'RESUME' : 'PAUSE';
        btnPause.style.color = isPaused ? 'var(--accent-red)' : 'var(--text-main)';
    });

    // Scope Mode Tabs
    function setScopeMode(mode) {
        scopeMode = mode;
        [tabTime, tabSpectrum, tabDual].forEach(t => t.classList.remove('active'));
        if (mode === 'time') tabTime.classList.add('active');
        else if (mode === 'spectrum') tabSpectrum.classList.add('active');
        else if (mode === 'dual') tabDual.classList.add('active');

        legendSpec.style.display = (mode === 'spectrum' || mode === 'dual') ? 'inline-flex' : 'none';
    }
    tabTime.addEventListener('click', () => setScopeMode('time'));
    tabSpectrum.addEventListener('click', () => setScopeMode('spectrum'));
    tabDual.addEventListener('click', () => setScopeMode('dual'));

    // Manual Gesture Buttons
    function sendManualCommand(cmdId) {
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({
                action: 'manual_command',
                command_id: cmdId
            }));
        }
    }

    document.querySelectorAll('.btn-gesture').forEach(btn => {
        btn.addEventListener('click', () => {
            const cmdId = parseInt(btn.getAttribute('data-cmd'), 10);
            sendManualCommand(cmdId);
        });
    });

    // Training Modal
    btnOpenTrain.addEventListener('click', () => modalTrain.classList.add('open'));
    btnCloseModal.addEventListener('click', () => modalTrain.classList.remove('open'));
    btnCloseModalDone.addEventListener('click', () => modalTrain.classList.remove('open'));
    modalTrain.addEventListener('click', (e) => {
        if (e.target === modalTrain) modalTrain.classList.remove('open');
    });

    btnCopyTrainCmd.addEventListener('click', () => {
        navigator.clipboard.writeText(trainCmdText.textContent).then(() => {
            btnCopyTrainCmd.textContent = 'COPIED!';
            setTimeout(() => { btnCopyTrainCmd.textContent = 'COPY'; }, 1500);
        });
    });

    // CSV Exporter
    btnExportCsv.addEventListener('click', () => {
        if (telemetryHistory.length === 0) {
            alert('No telemetry samples collected yet!');
            return;
        }
        const headers = ['timestamp', 'sequence', 'command', 'confidence', 'servo_angle', 'rms', 'mav', 'variance', 'wl', 'zc', 'ssc'];
        const csvRows = [headers.join(',')];
        for (const row of telemetryHistory) {
            csvRows.push([
                row.timestamp,
                row.seq,
                row.command,
                row.confidence,
                row.servo_angle,
                row.rms,
                row.mav,
                row.var,
                row.wl,
                row.zc,
                row.ssc
            ].join(','));
        }
        const blob = new Blob([csvRows.join('\n')], { type: 'text/csv' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `emg_telemetry_${Date.now()}.csv`;
        a.click();
        URL.revokeObjectURL(url);
    });

    // Scanlines Toggle
    btnToggleScanlines.addEventListener('click', () => {
        const isShown = screenScanlines.style.display !== 'none';
        screenScanlines.style.display = isShown ? 'none' : 'block';
    });

    // Audio Feedback Toggle
    btnToggleAudio.addEventListener('click', () => {
        audioEnabled = !audioEnabled;
        audioIcon.textContent = audioEnabled ? '🔊' : '🔇';
        if (audioEnabled) playBeep(520, 0.08);
    });

    // Global Keyboard Shortcuts
    window.addEventListener('keydown', (e) => {
        if (e.target.tagName === 'INPUT') return;
        if (e.key === '1') sendManualCommand(1);
        else if (e.key === '2') sendManualCommand(2);
        else if (e.key === '3') sendManualCommand(3);
        else if (e.key === '4') sendManualCommand(4);
        else if (e.code === 'Space') {
            e.preventDefault();
            sendManualCommand(0);
        } else if (e.key === 'p' || e.key === 'P') {
            btnPause.click();
        } else if (e.key === 't' || e.key === 'T') {
            modalTrain.classList.toggle('open');
        }
    });

    // Boot
    connectWS();
})();
