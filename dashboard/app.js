/**
 * EMG Actuator Telemetry Dashboard Client
 * 60 FPS Canvas Oscilloscope, Gauge Animator & WebSocket Telemetry
 */

(() => {
    'use strict';

    // Elements
    const canvas = document.getElementById('emg-canvas');
    const ctx = canvas.getContext('2d');
    const wsDot = document.getElementById('ws-dot');
    const wsStatusText = document.getElementById('ws-status-text');
    const udpDot = document.getElementById('udp-dot');
    const udpStatusText = document.getElementById('udp-status-text');
    const watchdogBadge = document.getElementById('watchdog-badge');
    const watchdogDot = document.getElementById('watchdog-dot');
    const watchdogText = document.getElementById('watchdog-text');

    const gestureBadge = document.getElementById('gesture-badge');
    const gestureName = document.getElementById('gesture-name');
    const gestureIcon = document.getElementById('gesture-icon');
    const confPct = document.getElementById('conf-pct');
    const confFill = document.getElementById('conf-fill');
    const streamTag = document.getElementById('stream-tag');

    const servoAngleDisplay = document.getElementById('servo-angle-display');
    const gaugeNeedle = document.getElementById('gauge-needle');
    const gaugeActiveArc = document.getElementById('gauge-active-arc');
    const handIconWrapper = document.getElementById('hand-icon-wrapper');
    const handStateText = document.getElementById('hand-state-text');

    const statPackets = document.getElementById('stat-packets');
    const statRate = document.getElementById('stat-rate');
    const statSeq = document.getElementById('stat-seq');
    const statCrc = document.getElementById('stat-crc');
    const statWatchdog = document.getElementById('stat-watchdog');

    const btnPause = document.getElementById('btn-pause-scope');

    // Feature element map
    const featuresMap = {
        rms: { val: document.getElementById('feat-rms'), bar: document.getElementById('meter-rms'), max: 1.0 },
        mav: { val: document.getElementById('feat-mav'), bar: document.getElementById('meter-mav'), max: 0.8 },
        variance: { val: document.getElementById('feat-var'), bar: document.getElementById('meter-var'), max: 0.5 },
        wl: { val: document.getElementById('feat-wl'), bar: document.getElementById('meter-wl'), max: 15.0 },
        zc: { val: document.getElementById('feat-zc'), bar: document.getElementById('meter-zc'), max: 30 },
        ssc: { val: document.getElementById('feat-ssc'), bar: document.getElementById('meter-ssc'), max: 30 }
    };

    const GESTURE_CONFIG = {
        0: { name: 'NONE', icon: '⚠️', desc: 'Safe State / Neutral (90°)', cls: 'gesture-NONE' },
        1: { name: 'RELAX', icon: '✋', desc: 'Resting Muscle / Neutral (90°)', cls: 'gesture-RELAX' },
        2: { name: 'GRASP', icon: '✊', desc: 'Closed Power Grip (0°)', cls: 'gesture-GRASP' },
        3: { name: 'OPEN', icon: '🖐️', desc: 'Full Hand Open (180°)', cls: 'gesture-OPEN' },
        4: { name: 'CLOSE', icon: '👊', desc: 'Closed Fist (0°)', cls: 'gesture-CLOSE' }
    };

    // State
    const MAX_BUFFER = 600;
    const rawBuffer = new Float32Array(MAX_BUFFER);
    const filteredBuffer = new Float32Array(MAX_BUFFER);
    let bufferIndex = 0;
    let isPaused = false;
    let ws = null;
    let currentAngle = 90;
    let targetAngle = 90;

    // Canvas sizing
    function resizeCanvas() {
        const rect = canvas.parentElement.getBoundingClientRect();
        canvas.width = rect.width * window.devicePixelRatio;
        canvas.height = rect.height * window.devicePixelRatio;
        ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
    }
    window.addEventListener('resize', resizeCanvas);
    resizeCanvas();

    // Oscilloscope Draw Loop (60 FPS)
    function drawOscilloscope() {
        requestAnimationFrame(drawOscilloscope);
        if (isPaused) return;

        const w = canvas.parentElement.clientWidth;
        const h = canvas.parentElement.clientHeight;
        const midY = h / 2;

        ctx.clearRect(0, 0, w, h);

        // Center zero line
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.12)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.moveTo(0, midY);
        ctx.lineTo(w, midY);
        ctx.stroke();

        const step = w / MAX_BUFFER;
        const scale = h * 0.38;

        // Trace 1: Raw Signal (Cyan)
        ctx.strokeStyle = '#38bdf8';
        ctx.lineWidth = 1.2;
        ctx.beginPath();
        for (let i = 0; i < MAX_BUFFER; i++) {
            const idx = (bufferIndex + i) % MAX_BUFFER;
            const x = i * step;
            const y = midY - (rawBuffer[idx] * scale);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();

        // Trace 2: Filtered Signal (Emerald Glow)
        ctx.strokeStyle = '#34d399';
        ctx.lineWidth = 2.0;
        ctx.shadowColor = '#34d399';
        ctx.shadowBlur = 6;
        ctx.beginPath();
        for (let i = 0; i < MAX_BUFFER; i++) {
            const idx = (bufferIndex + i) % MAX_BUFFER;
            const x = i * step;
            const y = midY - (filteredBuffer[idx] * scale);
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
        ctx.shadowBlur = 0; // Reset shadow
    }
    requestAnimationFrame(drawOscilloscope);

    // Smooth Servo Gauge Update
    function updateServoGauge(angle) {
        targetAngle = angle;
        currentAngle += (targetAngle - currentAngle) * 0.2;
        const rounded = Math.round(currentAngle);
        servoAngleDisplay.textContent = rounded;

        // SVG Needle rotation (-90 deg at 0°, 0 deg at 90°, +90 deg at 180°)
        const rotDeg = currentAngle - 90;
        gaugeNeedle.setAttribute('transform', `rotate(${rotDeg} 100 100)`);

        // Hand Visualizer styling
        handIconWrapper.classList.remove('hand-grip-closed', 'hand-grip-open');
        if (rounded <= 20) {
            handIconWrapper.classList.add('hand-grip-closed');
        } else if (rounded >= 160) {
            handIconWrapper.classList.add('hand-grip-open');
        }
    }

    // Process incoming telemetry frame
    function onTelemetryFrame(frame) {
        // 1. Push samples into oscilloscope ring buffer
        const raw = frame.raw_samples || [];
        const filt = frame.filtered_samples || [];
        for (let i = 0; i < raw.length; i++) {
            rawBuffer[bufferIndex] = raw[i];
            filteredBuffer[bufferIndex] = filt[i] || 0;
            bufferIndex = (bufferIndex + 1) % MAX_BUFFER;
        }

        // 2. Features
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

        // 3. Gesture Classification
        const cmdId = frame.command_id || 0;
        const gConf = GESTURE_CONFIG[cmdId] || GESTURE_CONFIG[0];
        gestureName.textContent = gConf.name;
        gestureIcon.textContent = gConf.icon;
        handStateText.textContent = gConf.desc;

        gestureBadge.className = 'gesture-badge ' + gConf.cls;

        const conf = frame.confidence || 0.0;
        const pct = Math.round(conf * 100);
        confPct.textContent = pct + '%';
        confFill.style.width = pct + '%';

        // 4. Actuator Gauge
        updateServoGauge(frame.servo_angle !== undefined ? frame.servo_angle : 90);

        // 5. Telemetry counters
        statPackets.textContent = frame.packets_received || 0;
        statRate.innerHTML = (frame.packet_rate_hz || 0).toFixed(1) + ' <small>Hz</small>';
        statSeq.textContent = frame.sequence_number || 0;

        if (frame.manual_override) {
            streamTag.textContent = 'MANUAL TEST';
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

        // 6. Safety Watchdog status
        if (frame.watchdog_timed_out) {
            watchdogBadge.classList.add('watchdog-alert');
            watchdogDot.className = 'badge-dot dot-offline';
            watchdogText.textContent = 'WATCHDOG: TIMEOUT (SAFE STATE)';
            statWatchdog.textContent = 'TRIGGERED (SAFE)';
            statWatchdog.className = 'stat-value status-warn';
        } else {
            watchdogBadge.classList.remove('watchdog-alert');
            watchdogDot.className = 'badge-dot dot-online';
            watchdogText.textContent = 'SAFETY WATCHDOG: ARMED';
            statWatchdog.textContent = 'ACTIVE (500ms)';
            statWatchdog.className = 'stat-value status-good';
        }
    }

    // WebSocket Connection
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
                console.error('Frame decode error:', err);
            }
        };

        ws.onclose = () => {
            wsDot.className = 'badge-dot dot-offline';
            wsStatusText.textContent = 'RECONNECTING...';
            setTimeout(connectWS, 1500);
        };

        ws.onerror = () => {
            ws.close();
        };
    }

    // Controls
    btnPause.addEventListener('click', () => {
        isPaused = !isPaused;
        btnPause.textContent = isPaused ? 'RESUME' : 'PAUSE';
        btnPause.style.color = isPaused ? '#f87171' : '#f8fafc';
    });

    // Manual Command Buttons
    document.querySelectorAll('.btn-gesture').forEach(btn => {
        btn.addEventListener('click', () => {
            const cmdId = parseInt(btn.getAttribute('data-cmd'), 10);
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    action: 'manual_command',
                    command_id: cmdId
                }));
            }
        });
    });

    // Start
    connectWS();
})();
