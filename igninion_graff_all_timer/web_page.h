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
        .container { max-width: 1000px; margin: 0 auto; flex: 1; }
        .tabs { display: flex; gap: 5px; margin-bottom: 20px; flex-wrap: wrap; }
        .tab-btn {
            background: #1a1a1a; border: none; padding: 12px 24px;
            border-radius: 12px; color: #888; font-weight: bold;
            cursor: pointer; transition: 0.3s;
        }
        .tab-btn.active { background: #00e676; color: #000; }
        .tab-btn:hover:not(.active) { background: #2a2a2a; color: #fff; }
        .tab-pane { display: none; }
        .tab-pane.active { display: block; }
        .card {
            background: #1a1a1a; border-radius: 16px;
            padding: 20px; margin-bottom: 20px;
        }
        .card-title {
            font-size: 1.2rem; font-weight: bold;
            color: #00e676; margin-bottom: 15px;
        }
        .telemetry-grid {
            display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 12px;
        }
        .telemetry-item {
            background: #0d0d0d; padding: 15px;
            border-radius: 12px; text-align: center;
        }
        .telemetry-value {
            font-size: 28px; font-weight: bold;
            font-family: monospace; color: #00e676;
        }
        .eq-scroll { overflow-x: auto; margin: 15px 0; }
        .eq-container { display: flex; gap: 10px; min-width: min-content; }
        .eq-item {
            display: flex; flex-direction: column;
            align-items: center; gap: 8px; min-width: 60px;
        }
        .eq-label {
            font-size: 11px; background: #2a2a2a;
            padding: 4px 8px; border-radius: 8px;
        }
        input[type="range"].eq-slider {
            -webkit-appearance: slider-vertical;
            width: 20px; height: 140px; background: #2a2a2a;
        }
        .eq-value { font-size: 12px; color: #00e676; font-weight: bold; }
        canvas {
            background: #0a0a0a; width: 100%; height: auto;
            border-radius: 12px; margin-top: 15px;
        }
        button {
            background: #00e676; color: #000; border: none;
            padding: 10px 20px; border-radius: 10px;
            font-weight: bold; cursor: pointer; margin: 5px; transition: 0.3s;
        }
        button:hover { opacity: 0.8; }
        .btn-outline { background: transparent; border: 1px solid #00e676; color: #00e676; }
        .btn-red { background: #f44336; color: #fff; }
        .btn-group { display: flex; gap: 10px; flex-wrap: wrap; margin-top: 15px; }
        .slider-group { margin-bottom: 20px; }
        .slider-header { display: flex; justify-content: space-between; margin-bottom: 8px; }
        input[type="range"] {
            width: 100%; height: 6px; -webkit-appearance: none;
            background: #2a2a2a; border-radius: 3px;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none; width: 20px; height: 20px;
            border-radius: 50%; cursor: pointer;
        }
        .slider-hard::-webkit-slider-thumb { background: #f44336; }
        .slider-soft::-webkit-slider-thumb { background: #ff9800; }
        .slider-wifi::-webkit-slider-thumb { background: #2196f3; }
        .slider-offset::-webkit-slider-thumb { background: #9c27b0; }
        .toast {
            position: fixed; bottom: 80px; right: 20px;
            background: #333; padding: 10px 20px;
            border-radius: 20px; opacity: 0;
            transition: opacity 0.3s; z-index: 1000;
        }
        .footer {
            text-align: center; padding: 20px 0 10px 0;
            color: #444; font-size: 14px; letter-spacing: 1px;
            border-top: 1px solid #1a1a1a; margin-top: 20px;
        }
        .offset-indicator {
            display: inline-block; padding: 4px 12px;
            border-radius: 20px; font-size: 14px;
            font-weight: bold; margin-left: 10px;
        }
        .offset-positive { background: #4caf50; color: #000; }
        .offset-negative { background: #f44336; color: #fff; }
        .offset-zero { background: #666; color: #fff; }
        @media (max-width: 600px) {
            .tab-btn { padding: 8px 16px; font-size: 12px; }
            .telemetry-value { font-size: 20px; }
        }
    </style>
</head>
<body>
<div class="container">
    <div class="card">
        <div style="display:flex;justify-content:space-between;align-items:center;">
            <span class="card-title">⚡ JetSurf Pro CDI</span>
            <div>
                <span id="offsetIndicator" class="offset-indicator offset-zero">0.0°</span>
                <span style="font-size:12px;color:#00e676;margin-left:10px;">ONLINE</span>
            </div>
        </div>
    </div>
    
    <div class="tabs">
        <button class="tab-btn active" data-tab="monitor">📊 Мониторинг</button>
        <button class="tab-btn" data-tab="tuning">🎛️ Настройка УОЗ</button>
        <button class="tab-btn" data-tab="limits">⛔ Ограничители</button>
        <button class="tab-btn" data-tab="sensor">📐 Коррекция</button>
        <button class="tab-btn" data-tab="service">🔧 Сервис</button>
    </div>
    
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
                    <div class="telemetry-value" id="mode_display" style="font-size:18px;">START</div>
                </div>
                <div class="telemetry-item">
                    <div>Статус</div>
                    <div class="telemetry-value" id="status_display" style="font-size:14px;">Ожидание</div>
                </div>
                <div class="telemetry-item">
                    <div>УОЗ</div>
                    <div class="telemetry-value"><span id="uoz_display">0.0</span>°</div>
                </div>
                <div class="telemetry-item">
                    <div>Коррекция</div>
                    <div class="telemetry-value"><span id="offset_display">0.0</span>°</div>
                </div>
            </div>
        </div>
        <div class="card">
            <div class="card-title">📈 Карта УОЗ</div>
            <canvas id="uozChart" width="800" height="250" style="width:100%;max-width:800px;margin:0 auto;display:block;"></canvas>
        </div>
        <div class="btn-group">
            <button id="saveMonitorBtn" class="btn-outline">💾 Сохранить во Flash</button>
        </div>
    </div>
    
    <div id="tab-tuning" class="tab-pane">
        <div class="card">
            <div class="card-title">🎛️ Углы опережения (0-35°)</div>
            <div class="eq-scroll"><div class="eq-container" id="eqContainer"></div></div>
            <div class="btn-group">
                <button id="applyTuningBtn">🚀 Применить</button>
                <button id="resetTuningBtn" class="btn-outline">🔄 Сбросить</button>
                <button id="saveTuningBtn" class="btn-outline">💾 Сохранить</button>
            </div>
        </div>
        <div class="card">
            <div class="card-title">📈 Просмотр карты УОЗ</div>
            <canvas id="uozChart2" width="800" height="250" style="width:100%;max-width:800px;margin:0 auto;display:block;"></canvas>
        </div>
    </div>
    
    <div id="tab-limits" class="tab-pane">
        <div class="card">
            <div class="card-title">⛔ Ограничитель оборотов</div>
            <div class="slider-group">
                <div class="slider-header">
                    <span>🔴 Жесткая отсечка</span>
                    <span><strong id="hardVal">15000</strong> RPM</span>
                </div>
                <input type="range" id="hardLimit" class="slider-hard" min="5000" max="16000" step="100" value="15000">
            </div>
            <div class="slider-group">
                <div class="slider-header">
                    <span>🟠 Мягкая отсечка</span>
                    <span><strong id="softVal">14600</strong> RPM</span>
                </div>
                <input type="range" id="softLimit" class="slider-soft" min="4000" max="15600" step="100" value="14600">
            </div>
            <div class="btn-group">
                <button id="applyLimitsBtn">🚀 Применить</button>
                <button id="saveLimitsBtn" class="btn-outline">💾 Сохранить</button>
            </div>
        </div>
    </div>
    
    <div id="tab-sensor" class="tab-pane">
        <div class="card">
            <div class="card-title">📐 Коррекция установки датчика Холла</div>
            <p style="margin-bottom:15px;color:#888;">
                Компенсация неточности физической установки датчика (±3°).<br>
                <strong>+</strong> = более раннее зажигание | <strong>-</strong> = более позднее
            </p>
            <div class="slider-group">
                <div class="slider-header">
                    <span>🟣 Коррекция угла:</span>
                    <span><strong id="sensorOffsetVal">0.0</strong>°</span>
                </div>
                <input type="range" id="sensorOffset" class="slider-offset" min="-3.0" max="3.0" step="0.1" value="0.0">
            </div>
            <div style="display:flex;gap:10px;flex-wrap:wrap;margin-top:15px;">
                <button id="applySensorBtn">✅ Применить</button>
                <button id="resetSensorBtn" class="btn-outline">↩️ Сбросить</button>
                <button id="saveSensorBtn" class="btn-outline">💾 Сохранить</button>
            </div>
            <div style="margin-top:20px;padding:15px;background:#0d0d0d;border-radius:12px;">
                <div style="color:#888;">Текущая коррекция:</div>
                <div style="font-size:24px;font-weight:bold;font-family:monospace;" id="currentOffsetDisplay">0.0°</div>
            </div>
        </div>
        <div class="card">
            <div class="card-title">📈 Карта УОЗ с коррекцией</div>
            <canvas id="uozChart3" width="800" height="250" style="width:100%;max-width:800px;margin:0 auto;display:block;"></canvas>
        </div>
    </div>
    
    <div id="tab-service" class="tab-pane">
        <div class="card">
            <div class="card-title">📶 Управление Wi-Fi</div>
            <p style="margin-bottom:15px;color:#888;">Wi-Fi отключается при оборотах выше порога (0 = всегда включен)</p>
            <div class="slider-group">
                <div class="slider-header">
                    <span>📡 Порог отключения:</span>
                    <span><strong id="wifiRpmDisplay">3000</strong> RPM</span>
                </div>
                <input type="range" id="wifiRpmSlider" class="slider-wifi" min="0" max="16000" step="100" value="3000">
            </div>
            <button id="applyWifiBtn">✅ Применить</button>
            <div id="wifiStatus" style="margin-top:15px;padding:10px;border-radius:8px;background:#0d0d0d;">
                <span id="wifiState">📶 Wi-Fi: ВКЛЮЧЕН</span>
            </div>
        </div>
        <div class="card">
            <div class="card-title">🔧 Просушка свечи</div>
            <p style="margin-bottom:15px;color:#888;">Эмуляция 800 RPM на 5 секунд</p>
            <button id="dryBtn" class="btn-red">🔥 Просушить свечу</button>
            <div id="dryStatus" style="margin-top:15px;color:#ff9800;"></div>
        </div>
        <div class="card">
            <div class="card-title">🔄 Обновление прошивки</div>
            <input type="file" id="otaFile" accept=".bin" style="margin-bottom:15px;"><br>
            <button id="otaBtn">🚀 Обновить</button>
            <div id="otaStatus" style="margin-top:15px;"></div>
        </div>
        <div class="card">
            <div class="card-title">ℹ️ Информация</div>
            <p>Wi-Fi: JetSurf_Smart_CDI<br>Пароль: 12345678<br>Версия: 2.2</p>
            <button id="saveServiceBtn" class="btn-outline">💾 Сохранить</button>
        </div>
    </div>
</div>

<div class="footer"><span>© MalkEvIch</span></div>
<div id="toast" class="toast"></div>

<script>
var mapData=[];
var defaultAngles=[10,12,14.5,18,22,24,23.5,22,20,18,16,15,14,13,12];
var rpmPoints=[1000,2000,3000,4000,5000,6000,7000,8000,9000,10000,11000,12000,13000,14000,15000];
var dryInterval=null;
var sensorOffset=0.0;

document.querySelectorAll('.tab-btn').forEach(function(btn){
    btn.addEventListener('click',function(){
        document.querySelectorAll('.tab-btn').forEach(function(b){b.classList.remove('active');});
        document.querySelectorAll('.tab-pane').forEach(function(p){p.classList.remove('active');});
        btn.classList.add('active');
        document.getElementById('tab-'+btn.dataset.tab).classList.add('active');
        drawAllCharts();
    });
});

function loadData(){
    fetch('/data').then(function(r){return r.json();}).then(function(d){
        document.getElementById('rpm_display').innerText=parseInt(d.rpm).toLocaleString();
        document.getElementById('mode_display').innerText=d.mode;
        document.getElementById('uoz_display').innerText=parseFloat(d.uoz||0).toFixed(1);
        if(d.offset!==undefined){
            sensorOffset=parseFloat(d.offset);
            document.getElementById('offset_display').innerText=sensorOffset.toFixed(1);
            updateOffsetIndicator(sensorOffset);
            var s=document.getElementById('sensorOffset');
            if(s&&document.activeElement!==s){
                s.value=sensorOffset;
                document.getElementById('sensorOffsetVal').innerText=sensorOffset.toFixed(1);
                document.getElementById('currentOffsetDisplay').innerText=sensorOffset.toFixed(1)+'°';
            }
        }
        var st=document.getElementById('status_display');
        if(d.mode==='HARD_LIMIT')st.innerText='❌ ОТСЕЧКА';
        else if(d.mode==='SOFT_LIMIT')st.innerText='⚠️ МЯГКАЯ';
        else if(d.mode==='DRY')st.innerText='🔥 ПРОСУШКА';
        else if(d.mode==='START')st.innerText='🚀 ПУСК';
        else st.innerText='✅ РАБОТА';
        var ws=document.getElementById('wifiState');
        if(ws){
            if(d.wifi){ws.innerText='📶 Wi-Fi: ВКЛЮЧЕН';ws.style.color='#00e676';}
            else{ws.innerText='📴 Wi-Fi: ОТКЛЮЧЕН';ws.style.color='#f44336';}
        }
    }).catch(function(e){console.log(e);});
}

function updateOffsetIndicator(offset){
    var ind=document.getElementById('offsetIndicator');
    if(!ind)return;
    ind.innerText=(offset>=0?'+':'')+offset.toFixed(1)+'°';
    ind.className='offset-indicator';
    if(offset>0.05)ind.classList.add('offset-positive');
    else if(offset<-0.05)ind.classList.add('offset-negative');
    else ind.classList.add('offset-zero');
}

function loadMap(){
    fetch('/map').then(function(r){return r.json();}).then(function(data){
        if(data&&data.length===15)mapData=data;
        else{mapData=[];for(var i=0;i<15;i++)mapData.push({rpm:rpmPoints[i],uoz:defaultAngles[i]});}
        buildEqSliders();drawAllCharts();
    }).catch(function(){
        mapData=[];for(var i=0;i<15;i++)mapData.push({rpm:rpmPoints[i],uoz:defaultAngles[i]});
        buildEqSliders();drawAllCharts();
    });
}

function loadLimits(){
    fetch('/limits').then(function(r){return r.json();}).then(function(data){
        document.getElementById('hardLimit').value=data.hard;
        document.getElementById('softLimit').value=data.soft;
        document.getElementById('hardVal').innerText=data.hard;
        document.getElementById('softVal').innerText=data.soft;
        if(data.offset!==undefined){
            sensorOffset=parseFloat(data.offset);
            document.getElementById('sensorOffset').value=sensorOffset;
            document.getElementById('sensorOffsetVal').innerText=sensorOffset.toFixed(1);
            document.getElementById('currentOffsetDisplay').innerText=sensorOffset.toFixed(1)+'°';
            updateOffsetIndicator(sensorOffset);
        }
    }).catch(function(e){console.log(e);});
}

function buildEqSliders(){
    var c=document.getElementById('eqContainer');
    if(!c)return;
    c.innerHTML='';
    var sd=document.createElement('div');
    sd.style.cssText='display:flex;gap:10px;min-width:min-content;';
    for(var i=0;i<mapData.length;i++){
        var d=document.createElement('div');
        d.className='eq-item';
        d.innerHTML='<div class="eq-label">'+mapData[i].rpm+'</div><input type="range" class="eq-slider" data-idx="'+i+'" min="0" max="35" step="0.5" value="'+mapData[i].uoz+'"><div class="eq-value" id="val_'+i+'">'+mapData[i].uoz.toFixed(1)+'°</div>';
        sd.appendChild(d);
    }
    c.appendChild(sd);
    document.querySelectorAll('.eq-slider').forEach(function(s){
        s.addEventListener('input',function(){
            var idx=parseInt(this.dataset.idx);
            var val=parseFloat(this.value);
            mapData[idx].uoz=val;
            document.getElementById('val_'+idx).innerText=val.toFixed(1)+'°';
            drawAllCharts();
        });
    });
}

function drawChart(canvasId,applyOffset){
    var canvas=document.getElementById(canvasId);
    if(!canvas)return;
    var ctx=canvas.getContext('2d');
    if(!ctx)return;
    var w=canvas.parentElement.clientWidth;
    canvas.width=Math.min(w,800);
    canvas.height=250;
    var W=canvas.width,H=canvas.height;
    var pl=45,pr=20,pt=20,pb=30;
    var cw=W-pl-pr,ch=H-pt-pb;
    ctx.clearRect(0,0,W,H);
    ctx.strokeStyle='#333';ctx.fillStyle='#888';ctx.font='10px monospace';
    for(var i=0;i<=7;i++){
        var y=pt+(i/7)*ch;
        ctx.beginPath();ctx.moveTo(pl,y);ctx.lineTo(W-pr,y);ctx.stroke();
        ctx.fillText(Math.round(35-(i/7)*35)+'°',8,y+3);
    }
    for(var i=0;i<mapData.length;i++){
        var x=pl+(i/(mapData.length-1))*cw;
        ctx.fillStyle='#888';ctx.font='9px monospace';
        ctx.fillText(mapData[i].rpm>=10000?(mapData[i].rpm/1000).toFixed(0)+'k':mapData[i].rpm,x-12,H-pb+12);
    }
    ctx.beginPath();
    ctx.strokeStyle=applyOffset?'#9c27b0':'#00e676';
    if(applyOffset)ctx.setLineDash([5,5]);
    else ctx.setLineDash([]);
    ctx.lineWidth=2;
    for(var i=0;i<mapData.length;i++){
        var x=pl+(i/(mapData.length-1))*cw;
        var uoz=applyOffset?mapData[i].uoz+sensorOffset:mapData[i].uoz;
        uoz=Math.max(0,Math.min(35,uoz));
        var y=pt+ch-(uoz/35)*ch;
        if(i===0)ctx.moveTo(x,y);
        else ctx.lineTo(x,y);
    }
    ctx.stroke();
    ctx.setLineDash([]);
    for(var i=0;i<mapData.length;i++){
        var x=pl+(i/(mapData.length-1))*cw;
        var uoz=applyOffset?mapData[i].uoz+sensorOffset:mapData[i].uoz;
        uoz=Math.max(0,Math.min(35,uoz));
        var y=pt+ch-(uoz/35)*ch;
        ctx.fillStyle='#fff';ctx.beginPath();ctx.arc(x,y,4,0,Math.PI*2);ctx.fill();
        ctx.fillStyle=applyOffset?'#9c27b0':'#00e676';ctx.beginPath();ctx.arc(x,y,2,0,Math.PI*2);ctx.fill();
    }
}

function drawAllCharts(){
    drawChart('uozChart',false);
    drawChart('uozChart2',false);
    drawChart('uozChart3',true);
}

function showToast(msg,isError){
    var t=document.getElementById('toast');
    t.innerText=msg;t.style.background=isError?'#f44336':'#333';
    t.style.opacity='1';setTimeout(function(){t.style.opacity='0';},2000);
}

function applyWifiSettings(){
    var rpm=parseInt(document.getElementById('wifiRpmSlider').value);
    var fd=new URLSearchParams();fd.append('wifi_off_rpm',rpm);
    fetch('/wifi',{method:'POST',body:fd}).then(function(r){return r.text();}).then(function(t){
        if(t==='OK'){showToast('Wi-Fi порог: '+rpm+' RPM',false);loadData();}
        else showToast('Ошибка',true);
    }).catch(function(){showToast('Ошибка сети',true);});
}

function applySensorOffset(){
    var offset=parseFloat(document.getElementById('sensorOffset').value);
    sensorOffset=offset;
    fetch('/offset',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'value='+offset})
    .then(function(r){return r.json();}).then(function(data){
        if(data.status==='ok'){
            updateOffsetIndicator(offset);
            document.getElementById('sensorOffsetVal').innerText=offset.toFixed(1);
            document.getElementById('currentOffsetDisplay').innerText=offset.toFixed(1)+'°';
            drawAllCharts();
            showToast('Коррекция: '+(offset>=0?'+':'')+offset.toFixed(1)+'°',false);
        }
    }).catch(function(){showToast('Ошибка',true);});
}

function resetSensorOffset(){
    document.getElementById('sensorOffset').value=0;
    sensorOffset=0;
    applySensorOffset();
}

function applySettings(){
    var fd=new URLSearchParams();
    fd.append('max_rpm',document.getElementById('hardLimit').value);
    fd.append('soft_rpm',document.getElementById('softLimit').value);
    fd.append('sensor_offset',sensorOffset);
    for(var i=0;i<mapData.length;i++)fd.append('uoz'+i,mapData[i].uoz);
    fetch('/apply',{method:'POST',body:fd}).then(function(){
        drawAllCharts();showToast('Применено',false);
    }).catch(function(){showToast('Ошибка',true);});
}

function saveSettings(){
    var fd=new URLSearchParams();
    fd.append('max_rpm',document.getElementById('hardLimit').value);
    fd.append('soft_rpm',document.getElementById('softLimit').value);
    fd.append('sensor_offset',sensorOffset);
    for(var i=0;i<mapData.length;i++)fd.append('uoz'+i,mapData[i].uoz);
    fetch('/apply',{method:'POST',body:fd}).then(function(){
        return fetch('/save');
    }).then(function(){showToast('Сохранено во Flash',false);})
    .catch(function(){showToast('Ошибка',true);});
}

function resetDefaults(){
    if(!confirm('Сбросить все настройки?'))return;
    for(var i=0;i<15;i++)mapData[i].uoz=defaultAngles[i];
    document.getElementById('hardLimit').value=15000;
    document.getElementById('softLimit').value=14600;
    document.getElementById('hardVal').innerText=15000;
    document.getElementById('softVal').innerText=14600;
    for(var i=0;i<15;i++){
        var s=document.querySelector('.eq-slider[data-idx="'+i+'"]');
        if(s)s.value=mapData[i].uoz;
        var v=document.getElementById('val_'+i);
        if(v)v.innerText=mapData[i].uoz.toFixed(1)+'°';
    }
    drawAllCharts();applySettings();
}

function syncLimits(){
    var hard=parseInt(document.getElementById('hardLimit').value);
    var soft=parseInt(document.getElementById('softLimit').value);
    if(soft>hard-400){soft=hard-400;document.getElementById('softLimit').value=soft;}
    if(soft<4000){soft=4000;document.getElementById('softLimit').value=soft;}
    document.getElementById('hardVal').innerText=hard;
    document.getElementById('softVal').innerText=soft;
}

function startDryMode(){
    fetch('/dry').then(function(r){return r.text();}).then(function(msg){
        showToast(msg,false);
        var sd=document.getElementById('dryStatus');
        sd.innerHTML='🔥 Просушка активна... 5 секунд';
        if(dryInterval)clearInterval(dryInterval);
        dryInterval=setInterval(function(){
            fetch('/dry/status').then(function(r){return r.json();}).then(function(d){
                if(!d.active){clearInterval(dryInterval);sd.innerHTML='✅ Завершена';}
            }).catch(function(){});
        },500);
    }).catch(function(){showToast('Ошибка',true);});
}

function startOTA(){
    var f=document.getElementById('otaFile').files[0];
    if(!f){showToast('Выберите файл',true);return;}
    var fd=new FormData();fd.append('firmware',f);
    var sd=document.getElementById('otaStatus');
    sd.innerHTML='<span style="color:#ff9800;">⏳ Загрузка...</span>';
    fetch('/update',{method:'POST',body:fd}).then(function(r){return r.text();}).then(function(t){
        if(t==='OK'){sd.innerHTML='<span style="color:#00e676;">✅ Успешно! Перезагрузка...</span>';setTimeout(function(){location.reload();},5000);}
        else sd.innerHTML='<span style="color:#f44336;">❌ Ошибка: '+t+'</span>';
    }).catch(function(){sd.innerHTML='<span style="color:#f44336;">❌ Ошибка сети</span>';});
}

loadMap();loadLimits();loadData();
setInterval(loadData,500);
window.addEventListener('resize',function(){drawAllCharts();});

document.getElementById('applyTuningBtn').addEventListener('click',applySettings);
document.getElementById('resetTuningBtn').addEventListener('click',resetDefaults);
document.getElementById('saveTuningBtn').addEventListener('click',saveSettings);
document.getElementById('applyLimitsBtn').addEventListener('click',applySettings);
document.getElementById('saveLimitsBtn').addEventListener('click',saveSettings);
document.getElementById('saveMonitorBtn').addEventListener('click',saveSettings);
document.getElementById('saveServiceBtn').addEventListener('click',saveSettings);
document.getElementById('applySensorBtn').addEventListener('click',applySensorOffset);
document.getElementById('resetSensorBtn').addEventListener('click',resetSensorOffset);
document.getElementById('saveSensorBtn').addEventListener('click',saveSettings);
document.getElementById('dryBtn').addEventListener('click',startDryMode);
document.getElementById('otaBtn').addEventListener('click',startOTA);
document.getElementById('applyWifiBtn').addEventListener('click',applyWifiSettings);
document.getElementById('hardLimit').addEventListener('input',syncLimits);
document.getElementById('softLimit').addEventListener('input',function(){
    var hard=parseInt(document.getElementById('hardLimit').value);
    var soft=parseInt(this.value);
    if(soft>hard-400){soft=hard-400;this.value=soft;}
    document.getElementById('softVal').innerText=soft;
});
document.getElementById('wifiRpmSlider').addEventListener('input',function(){
    document.getElementById('wifiRpmDisplay').innerText=this.value;
});
document.getElementById('sensorOffset').addEventListener('input',function(){
    document.getElementById('sensorOffsetVal').innerText=parseFloat(this.value).toFixed(1);
});
</script>
</body>
</html>
)rawliteral";

#endif