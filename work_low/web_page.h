#ifndef WEB_PAGE_H
#define WEB_PAGE_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>JetSurf Pro CDI</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #0a0a0a;
            color: #e0e0e0;
            padding: 10px;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
        }
        .container {
            max-width: 1000px;
            margin: 0 auto;
            flex: 1;
        }
        
        /* Вкладки */
        .tabs {
            display: flex;
            gap: 5px;
            margin-bottom: 20px;
            flex-wrap: wrap;
        }
        .tab-btn {
            background: #1a1a1a;
            border: none;
            padding: 12px 24px;
            border-radius: 12px;
            color: #888;
            font-weight: bold;
            cursor: pointer;
            transition: 0.3s;
        }
        .tab-btn.active {
            background: #00e676;
            color: #000;
        }
        .tab-btn:hover:not(.active) {
            background: #2a2a2a;
            color: #fff;
        }
        
        /* Панели */
        .tab-pane { display: none; }
        .tab-pane.active { display: block; }
        
        /* Карточки */
        .card {
            background: #1a1a1a;
            border-radius: 16px;
            padding: 20px;
            margin-bottom: 20px;
        }
        .card-title {
            font-size: 1.2rem;
            font-weight: bold;
            color: #00e676;
            margin-bottom: 15px;
        }
        
        /* Телеметрия */
        .telemetry-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 12px;
        }
        .telemetry-item {
            background: #0d0d0d;
            padding: 15px;
            border-radius: 12px;
            text-align: center;
        }
        .telemetry-value {
            font-size: 28px;
            font-weight: bold;
            font-family: monospace;
            color: #00e676;
        }
        
        /* Эквалайзер */
        .eq-scroll {
            overflow-x: auto;
            margin: 15px 0;
        }
        .eq-container {
            display: flex;
            gap: 10px;
            min-width: min-content;
        }
        .eq-item {
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 8px;
            min-width: 60px;
        }
        .eq-label {
            font-size: 11px;
            background: #2a2a2a;
            padding: 4px 8px;
            border-radius: 8px;
        }
        input[type="range"].eq-slider {
            -webkit-appearance: slider-vertical;
            width: 20px;
            height: 140px;
            background: #2a2a2a;
        }
        .eq-value {
            font-size: 12px;
            color: #00e676;
            font-weight: bold;
        }
        
        /* График */
        canvas {
            background: #0a0a0a;
            width: 100%;
            height: auto;
            border-radius: 12px;
            margin-top: 15px;
        }
        
        /* Кнопки */
        button {
            background: #00e676;
            color: #000;
            border: none;
            padding: 10px 20px;
            border-radius: 10px;
            font-weight: bold;
            cursor: pointer;
            margin: 5px;
            transition: 0.3s;
        }
        button:hover { opacity: 0.8; }
        .btn-outline {
            background: transparent;
            border: 1px solid #00e676;
            color: #00e676;
        }
        .btn-red {
            background: #f44336;
            color: #fff;
        }
        .btn-group {
            display: flex;
            gap: 10px;
            flex-wrap: wrap;
            margin-top: 15px;
        }
        
        /* Ползунки отсечек */
        .slider-group { margin-bottom: 20px; }
        .slider-header {
            display: flex;
            justify-content: space-between;
            margin-bottom: 8px;
        }
        input[type="range"] {
            width: 100%;
            height: 6px;
            -webkit-appearance: none;
            background: #2a2a2a;
            border-radius: 3px;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            cursor: pointer;
        }
        .slider-hard::-webkit-slider-thumb { background: #f44336; }
        .slider-soft::-webkit-slider-thumb { background: #ff9800; }
        .slider-wifi::-webkit-slider-thumb { background: #2196f3; }
        
        .toast {
            position: fixed;
            bottom: 80px;
            right: 20px;
            background: #333;
            padding: 10px 20px;
            border-radius: 20px;
            opacity: 0;
            transition: opacity 0.3s;
            z-index: 1000;
        }
        
        /* Футер */
        .footer {
            text-align: center;
            padding: 20px 0 10px 0;
            color: #444;
            font-size: 14px;
            letter-spacing: 1px;
            border-top: 1px solid #1a1a1a;
            margin-top: 20px;
        }
        .footer span {
            color: #666;
        }
        
        @media (max-width: 600px) {
            .tab-btn { padding: 8px 16px; font-size: 12px; }
            .telemetry-value { font-size: 20px; }
        }
    </style>
</head>
<body>
<div class="container">
    
    <!-- Заголовок -->
    <div class="card">
        <div style="display: flex; justify-content: space-between; align-items: center;">
            <span class="card-title">⚡ JetSurf Pro CDI</span>
            <div style="font-size: 12px; color: #00e676;">ONLINE</div>
        </div>
    </div>
    
    <!-- Вкладки -->
    <div class="tabs">
        <button class="tab-btn active" data-tab="monitor">📊 Мониторинг</button>
        <button class="tab-btn" data-tab="tuning">🎛️ Настройка УОЗ</button>
        <button class="tab-btn" data-tab="limits">⛔ Ограничители</button>
        <button class="tab-btn" data-tab="service">🔧 Сервис</button>
    </div>
    
    <!-- Вкладка 1: Мониторинг -->
    <div id="tab-monitor" class="tab-pane active">
        <div class="card">
            <div class="card-title">📡 Текущие показатели</div>
            <div class="telemetry-grid">
                <div class="telemetry-item">
                    <div>Обороты</div>
                    <div class="telemetry-value"><span id="rpm_display">0</span> RPM</div>
                </div>
                <div class="telemetry-item">
                    <div>Режим</div>
                    <div class="telemetry-value" id="mode_display" style="font-size: 18px;">START</div>
                </div>
                <div class="telemetry-item">
                    <div>Статус</div>
                    <div class="telemetry-value" id="status_display" style="font-size: 14px;">Ожидание</div>
                </div>
            </div>
        </div>
        
        <div class="card">
            <div class="card-title">📈 Карта УОЗ</div>
            <canvas id="uozChart" width="800" height="250" style="width:100%; max-width:800px; margin:0 auto; display:block"></canvas>
        </div>
        
        <div class="btn-group">
            <button id="saveMonitorBtn" class="btn-outline">💾 Сохранить во Flash</button>
        </div>
    </div>
    
    <!-- Вкладка 2: Настройка УОЗ + График -->
    <div id="tab-tuning" class="tab-pane">
        <div class="card">
            <div class="card-title">🎛️ Углы опережения (0-35°)</div>
            <div class="eq-scroll">
                <div class="eq-container" id="eqContainer"></div>
            </div>
            <div class="btn-group">
                <button id="applyTuningBtn">🚀 Применить</button>
                <button id="resetTuningBtn" class="btn-outline">🔄 Сбросить к Default</button>
                <button id="saveTuningBtn" class="btn-outline">💾 Сохранить во Flash</button>
            </div>
        </div>
        
        <div class="card">
            <div class="card-title">📈 Просмотр карты УОЗ</div>
            <canvas id="uozChart2" width="800" height="250" style="width:100%; max-width:800px; margin:0 auto; display:block"></canvas>
        </div>
    </div>
    
    <!-- Вкладка 3: Ограничители -->
    <div id="tab-limits" class="tab-pane">
        <div class="card">
            <div class="card-title">⛔ Ограничитель оборотов</div>
            
            <div class="slider-group">
                <div class="slider-header">
                    <span>🔴 Жесткая отсечка (нет искры)</span>
                    <span><strong id="hardVal">15000</strong> RPM</span>
                </div>
                <input type="range" id="hardLimit" class="slider-hard" min="5000" max="36000" step="100" value="15000">
            </div>
            
            <div class="slider-group">
                <div class="slider-header">
                    <span>🟠 Мягкая отсечка (спад угла)</span>
                    <span><strong id="softVal">20600</strong> RPM</span>
                </div>
                <input type="range" id="softLimit" class="slider-soft" min="4000" max="20600" step="100" value="14600">
            </div>
            
            <div class="btn-group">
                <button id="applyLimitsBtn">🚀 Применить</button>
                <button id="saveLimitsBtn" class="btn-outline">💾 Сохранить во Flash</button>
            </div>
        </div>
    </div>
    
    <!-- Вкладка 4: Сервис -->
    <div id="tab-service" class="tab-pane">
        <div class="card">
            <div class="card-title">📶 Управление Wi-Fi</div>
            <p style="margin-bottom: 15px; color: #888;">Задайте порог оборотов, при котором Wi-Fi будет автоматически отключаться (0 = всегда включен).</p>
            
            <div class="slider-group">
                <div class="slider-header">
                    <span>📡 Отключить Wi-Fi при оборотах выше:</span>
                    <span><strong id="wifiRpmDisplay">3000</strong> RPM</span>
                </div>
                <input type="range" id="wifiRpmSlider" class="slider-wifi" min="0" max="16000" step="100" value="3000">
            </div>
            
            <div style="display: flex; gap: 10px; flex-wrap: wrap;">
                <button id="applyWifiBtn">✅ Применить</button>
                <button id="resetWifiBtn" class="btn-outline">↩️ Сбросить к 3000</button>
            </div>
            
            <div id="wifiStatus" style="margin-top: 15px; padding: 10px; border-radius: 8px; background: #0d0d0d;">
                <span id="wifiState">📶 Wi-Fi: ВКЛЮЧЕН</span>
            </div>
        </div>
        
        <div class="card">
            <div class="card-title">🔧 Просушка свечи</div>
            <p style="margin-bottom: 15px; color: #888;">Включает эмулятор оборотов (800 RPM) на 5 секунд для просушки свечи.</p>
            <button id="dryBtn" class="btn-red">🔥 Просушить свечу</button>
            <div id="dryStatus" style="margin-top: 15px; color: #ff9800;"></div>
        </div>
        
        <div class="card">
            <div class="card-title">🔄 Обновление прошивки</div>
            <p style="margin-bottom: 15px; color: #888;">Загрузите файл прошивки (.bin) для OTA обновления.</p>
            <input type="file" id="otaFile" accept=".bin" style="margin-bottom: 15px;">
            <button id="otaBtn">🚀 Загрузить и обновить</button>
            <div id="otaStatus" style="margin-top: 15px;"></div>
        </div>
        
        <div class="card">
            <div class="card-title">ℹ️ Информация</div>
            <p>Wi-Fi: JetSurf_Smart_CDI<br>Пароль: 12345678<br>Версия: 2.1</p>
            <button id="saveServiceBtn" class="btn-outline">💾 Сохранить во Flash</button>
        </div>
    </div>
</div>

<!-- Футер -->
<div class="footer">
    <span>© MalkEvIch</span>
</div>

<div id="toast" class="toast"></div>

<script>
    // ========== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ==========
    let mapData = [];
    let defaultAngles = [10, 12, 14.5, 18, 22, 24, 23.5, 22, 20, 18, 16, 15, 14, 13, 12];
    let rpmPoints = [1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 11000, 12000, 13000, 14000, 15000];
    let dryInterval = null;
    
    // ========== ВКЛАДКИ ==========
    document.querySelectorAll('.tab-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
            btn.classList.add('active');
            document.getElementById(`tab-${btn.dataset.tab}`).classList.add('active');
            drawAllCharts();
        });
    });
    
    // ========== ЗАГРУЗКА ДАННЫХ ==========
    function loadData() {
        fetch('/data')
            .then(r => r.json())
            .then(d => {
                document.getElementById('rpm_display').innerText = parseInt(d.rpm).toLocaleString();
                document.getElementById('mode_display').innerText = d.mode;
                let status = document.getElementById('status_display');
                if (d.mode === 'HARD_LIMIT') status.innerText = '❌ ОТСЕЧКА';
                else if (d.mode === 'SOFT_LIMIT') status.innerText = '⚠️ МЯГКАЯ';
                else if (d.mode === 'DRY') status.innerText = '🔥 ПРОСУШКА';
                else if (d.mode === 'START') status.innerText = '🚀 ПУСК';
                else status.innerText = '✅ РАБОТА';
                
                // Wi-Fi статус
                let wifiState = document.getElementById('wifiState');
                if (wifiState) {
                    if (d.wifi_enabled) {
                        wifiState.innerText = '📶 Wi-Fi: ВКЛЮЧЕН';
                        wifiState.style.color = '#00e676';
                    } else {
                        wifiState.innerText = '📴 Wi-Fi: ОТКЛЮЧЕН (режим гонки)';
                        wifiState.style.color = '#f44336';
                    }
                }
                if (d.wifi_off_rpm !== undefined) {
                    let slider = document.getElementById('wifiRpmSlider');
                    let display = document.getElementById('wifiRpmDisplay');
                    if (slider && slider.value != d.wifi_off_rpm) {
                        slider.value = d.wifi_off_rpm;
                        if (display) display.innerText = d.wifi_off_rpm;
                    }
                }
            }).catch(e => console.log(e));
    }
    
    function loadMap() {
        fetch('/map')
            .then(r => r.json())
            .then(data => {
                if (data && data.length === 15) mapData = data;
                else for(let i=0;i<15;i++) mapData.push({rpm: rpmPoints[i], uoz: defaultAngles[i]});
                buildEqSliders();
                drawAllCharts();
            }).catch(() => {
                for(let i=0;i<15;i++) mapData.push({rpm: rpmPoints[i], uoz: defaultAngles[i]});
                buildEqSliders();
                drawAllCharts();
            });
    }
    
    function loadLimits() {
        fetch('/limits')
            .then(r => r.json())
            .then(data => {
                document.getElementById('hardLimit').value = data.hard;
                document.getElementById('softLimit').value = data.soft;
                document.getElementById('hardVal').innerText = data.hard;
                document.getElementById('softVal').innerText = data.soft;
            }).catch(e => console.log(e));
    }
    
    // ========== ЭКВАЛАЙЗЕР ==========
    function buildEqSliders() {
        const container = document.getElementById('eqContainer');
        if (!container) return;
        container.innerHTML = '';
        for (let i = 0; i < mapData.length; i++) {
            const div = document.createElement('div');
            div.className = 'eq-item';
            div.innerHTML = `
                <div class="eq-label">${mapData[i].rpm}</div>
                <input type="range" class="eq-slider" data-idx="${i}" min="0" max="35" step="0.5" value="${mapData[i].uoz}">
                <div class="eq-value" id="val_${i}">${mapData[i].uoz.toFixed(1)}°</div>
            `;
            container.appendChild(div);
        }
        document.querySelectorAll('.eq-slider').forEach(slider => {
            slider.addEventListener('input', function() {
                let idx = parseInt(this.dataset.idx);
                let val = parseFloat(this.value);
                mapData[idx].uoz = val;
                document.getElementById(`val_${idx}`).innerText = val.toFixed(1) + '°';
                drawAllCharts();
            });
        });
    }
    
    // ========== ГРАФИКИ ==========
    function drawChart(canvasId) {
        let canvas = document.getElementById(canvasId);
        if (!canvas) return;
        let ctx = canvas.getContext('2d');
        if (!ctx) return;
        
        let w = canvas.parentElement.clientWidth;
        canvas.width = Math.min(w, 800);
        canvas.height = 250;
        let width = canvas.width, height = canvas.height;
        let padLeft = 45, padRight = 20, padTop = 20, padBottom = 30;
        let chartW = width - padLeft - padRight;
        let chartH = height - padTop - padBottom;
        ctx.clearRect(0, 0, width, height);
        
        ctx.strokeStyle = '#333';
        ctx.fillStyle = '#888';
        ctx.font = '10px monospace';
        for (let i = 0; i <= 7; i++) {
            let y = padTop + (i/7)*chartH;
            let angle = Math.round(35 - (i/7)*35);
            ctx.beginPath();
            ctx.moveTo(padLeft, y);
            ctx.lineTo(width-padRight, y);
            ctx.stroke();
            ctx.fillText(angle+'°', 8, y+3);
        }
        
        for (let i = 0; i < mapData.length; i++) {
            let x = padLeft + (i/(mapData.length-1))*chartW;
            ctx.fillStyle = '#888';
            ctx.font = '9px monospace';
            let txt = mapData[i].rpm >= 10000 ? (mapData[i].rpm/1000).toFixed(0)+'k' : mapData[i].rpm;
            ctx.fillText(txt, x-12, height-padBottom+12);
        }
        
        ctx.beginPath();
        ctx.strokeStyle = '#00e676';
        ctx.lineWidth = 2;
        for (let i = 0; i < mapData.length; i++) {
            let x = padLeft + (i/(mapData.length-1))*chartW;
            let y = padTop + chartH - (mapData[i].uoz/35)*chartH;
            if (i === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }
        ctx.stroke();
        
        for (let i = 0; i < mapData.length; i++) {
            let x = padLeft + (i/(mapData.length-1))*chartW;
            let y = padTop + chartH - (mapData[i].uoz/35)*chartH;
            ctx.fillStyle = '#fff';
            ctx.beginPath();
            ctx.arc(x, y, 4, 0, Math.PI*2);
            ctx.fill();
            ctx.fillStyle = '#00e676';
            ctx.beginPath();
            ctx.arc(x, y, 2, 0, Math.PI*2);
            ctx.fill();
        }
    }
    
    function drawAllCharts() {
        drawChart('uozChart');
        drawChart('uozChart2');
    }
    
    // ========== УВЕДОМЛЕНИЯ ==========
    function showToast(msg, isError = false) {
        let toast = document.getElementById('toast');
        toast.innerText = msg;
        toast.style.background = isError ? '#f44336' : '#333';
        toast.style.opacity = '1';
        setTimeout(() => toast.style.opacity = '0', 2000);
    }
    
    // ========== Wi-Fi ==========
    function applyWifiSettings() {
        let rpm = parseInt(document.getElementById('wifiRpmSlider').value);
        let formData = new URLSearchParams();
        formData.append('wifi_off_rpm', rpm);
        fetch('/wifi', { method: 'POST', body: formData })
            .then(r => r.text())
            .then(t => {
                if (t === 'OK') {
                    showToast('✅ Порог Wi-Fi: ' + rpm + ' RPM');
                    loadData();
                } else {
                    showToast('❌ Ошибка', true);
                }
            })
            .catch(() => showToast('❌ Ошибка сети', true));
    }
    
    function resetWifiSettings() {
        document.getElementById('wifiRpmSlider').value = 3000;
        document.getElementById('wifiRpmDisplay').innerText = 3000;
        applyWifiSettings();
    }
    
    // ========== ПРИМЕНЕНИЕ НАСТРОЕК ==========
    function applySettings() {
        let formData = new URLSearchParams();
        formData.append('max_rpm', document.getElementById('hardLimit').value);
        formData.append('soft_rpm', document.getElementById('softLimit').value);
        for (let i = 0; i < mapData.length; i++) formData.append(`uoz${i}`, mapData[i].uoz);
        fetch('/apply', { method: 'POST', body: formData })
            .then(() => showToast('✅ Настройки применены'))
            .catch(() => showToast('❌ Ошибка', true));
    }
    
    function saveSettings() {
        let formData = new URLSearchParams();
        formData.append('max_rpm', document.getElementById('hardLimit').value);
        formData.append('soft_rpm', document.getElementById('softLimit').value);
        for (let i = 0; i < mapData.length; i++) formData.append(`uoz${i}`, mapData[i].uoz);
        fetch('/apply', { method: 'POST', body: formData })
            .then(() => fetch('/save'))
            .then(() => showToast('💾 Сохранено во Flash'))
            .catch(() => showToast('❌ Ошибка', true));
    }
    
    function resetDefaults() {
        if (!confirm('Сбросить все настройки?')) return;
        for (let i = 0; i < 15; i++) mapData[i].uoz = defaultAngles[i];
        document.getElementById('hardLimit').value = 15000;
        document.getElementById('softLimit').value = 14600;
        document.getElementById('hardVal').innerText = 15000;
        document.getElementById('softVal').innerText = 14600;
        for (let i = 0; i < 15; i++) {
            let slider = document.querySelector(`.eq-slider[data-idx="${i}"]`);
            if (slider) slider.value = mapData[i].uoz;
            let valSpan = document.getElementById(`val_${i}`);
            if (valSpan) valSpan.innerText = mapData[i].uoz.toFixed(1) + '°';
        }
        drawAllCharts();
        applySettings();
    }
    
    function syncLimits() {
        let hard = parseInt(document.getElementById('hardLimit').value);
        let soft = parseInt(document.getElementById('softLimit').value);
        if (soft > hard - 400) { soft = hard - 400; document.getElementById('softLimit').value = soft; }
        if (soft < 4000) { soft = 4000; document.getElementById('softLimit').value = soft; }
        document.getElementById('hardVal').innerText = hard;
        document.getElementById('softVal').innerText = soft;
    }
    
    // ========== ПРОСУШКА СВЕЧИ ==========
    function startDryMode() {
        fetch('/dry')
            .then(r => r.text())
            .then(msg => {
                showToast(msg, false);
                let statusDiv = document.getElementById('dryStatus');
                statusDiv.innerHTML = '🔥 Просушка активна... 5 секунд';
                if (dryInterval) clearInterval(dryInterval);
                dryInterval = setInterval(() => {
                    fetch('/dry/status')
                        .then(r => r.json())
                        .then(d => {
                            if (!d.active) {
                                clearInterval(dryInterval);
                                statusDiv.innerHTML = '✅ Просушка завершена';
                                setTimeout(() => statusDiv.innerHTML = '', 3000);
                            }
                        }).catch(() => {});
                }, 500);
            }).catch(() => showToast('❌ Ошибка', true));
    }
    
    // ========== OTA ==========
    function startOTA() {
        let file = document.getElementById('otaFile').files[0];
        if (!file) { showToast('Выберите файл', true); return; }
        let formData = new FormData();
        formData.append('firmware', file);
        let statusDiv = document.getElementById('otaStatus');
        statusDiv.innerHTML = '<div style="color:#ff9800">⏳ Загрузка...</div>';
        fetch('/update', { method: 'POST', body: formData })
            .then(r => r.text())
            .then(t => {
                if (t === 'OK') {
                    statusDiv.innerHTML = '<div style="color:#00e676">✅ Успешно! Перезагрузка...</div>';
                    setTimeout(() => location.reload(), 5000);
                } else {
                    statusDiv.innerHTML = '<div style="color:#f44336">❌ Ошибка: ' + t + '</div>';
                }
            }).catch(() => {
                statusDiv.innerHTML = '<div style="color:#f44336">❌ Ошибка сети</div>';
            });
    }
    
    // ========== ИНИЦИАЛИЗАЦИЯ ==========
    loadMap();
    loadLimits();
    loadData();
    setInterval(loadData, 500);
    window.addEventListener('resize', () => drawAllCharts());
    
    // Кнопки на вкладке Tuning
    document.getElementById('applyTuningBtn')?.addEventListener('click', applySettings);
    document.getElementById('resetTuningBtn')?.addEventListener('click', resetDefaults);
    document.getElementById('saveTuningBtn')?.addEventListener('click', saveSettings);
    
    // Кнопки на вкладке Limits
    document.getElementById('applyLimitsBtn')?.addEventListener('click', applySettings);
    document.getElementById('saveLimitsBtn')?.addEventListener('click', saveSettings);
    document.getElementById('hardLimit')?.addEventListener('input', syncLimits);
    document.getElementById('softLimit')?.addEventListener('input', function() {
        let hard = parseInt(document.getElementById('hardLimit').value);
        let soft = parseInt(this.value);
        if (soft > hard - 400) { soft = hard - 400; this.value = soft; }
        document.getElementById('softVal').innerText = soft;
    });
    
    // Кнопки на вкладке Monitor
    document.getElementById('saveMonitorBtn')?.addEventListener('click', saveSettings);
    
    // Кнопки на вкладке Service
    document.getElementById('saveServiceBtn')?.addEventListener('click', saveSettings);
    document.getElementById('dryBtn')?.addEventListener('click', startDryMode);
    document.getElementById('otaBtn')?.addEventListener('click', startOTA);
    
    // Wi-Fi
    document.getElementById('wifiRpmSlider')?.addEventListener('input', function() {
        document.getElementById('wifiRpmDisplay').innerText = this.value;
    });
    document.getElementById('applyWifiBtn')?.addEventListener('click', applyWifiSettings);
    document.getElementById('resetWifiBtn')?.addEventListener('click', resetWifiSettings);
</script>
</body>
</html>
)rawliteral";
//gh
#endif