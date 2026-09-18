#ifndef WEB_PAGE_H
#define WEB_PAGE_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>JetSurf CDI</title>
    <style>
        * { margin:0; padding:0; box-sizing:border-box; }
        body {
            font-family: Arial, sans-serif;
            background: #0a0a0a;
            color: #0f0;
            padding: 10px;
            line-height: 1.4;
        }
        h1 { text-align: center; color: #0f0; margin: 15px 0; font-size: 1.6em; }
        .card {
            background: #1f1f1f;
            border-radius: 12px;
            padding: 15px;
            margin: 12px 0;
            box-shadow: 0 4px 8px rgba(0,0,0,0.5);
        }
        .map-buttons {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
            justify-content: center;
        }
        button {
            flex: 1;
            min-width: 80px;
            padding: 12px 8px;
            background: #004400;
            color: #0f0;
            border: 1px solid #0f0;
            border-radius: 8px;
            font-size: 1em;
            cursor: pointer;
        }
        button.active {
            background: #00ff00;
            color: black;
            font-weight: bold;
        }
        button.preset {
            border-color: #008800;
        }
        button.custom {
            border-color: #ff8800;
            color: #ff8800;
        }
        button.custom.active {
            background: #ff8800;
            color: black;
        }
        #live {
            font-size: 1.4em;
            font-weight: bold;
            text-align: center;
            padding: 12px;
            background: #112200;
            border-radius: 8px;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 10px;
        }
        th, td {
            padding: 10px 6px;
            text-align: center;
            border-bottom: 1px solid #333;
        }
        input[type="number"] {
            width: 65px;
            background: #222;
            color: #0f0;
            border: 1px solid #555;
            border-radius: 4px;
            text-align: center;
            font-size: 1.1em;
        }
        input[type="number"]:disabled {
            background: #111;
            color: #666;
            border-color: #333;
        }
        .footer-buttons {
            display: flex;
            gap: 10px;
            margin-top: 15px;
        }
        .footer-buttons button {
            flex: 1;
            padding: 14px;
            font-size: 1.1em;
        }
        .warning {
            color: #ff8800;
            font-size: 0.9em;
            text-align: center;
            margin-top: 8px;
        }
    </style>
</head>
<body>
    <h1>🚀 JetSurf CDI</h1>
    
    <div class="card">
        <h2>Выбор карты</h2>
        <div class="map-buttons">
            <button onclick="selectMap(0)" id="btn0" class="preset">🔒 Нормальная</button>
            <button onclick="selectMap(1)" id="btn1" class="preset">🔒 Спорт</button>
            <button onclick="selectMap(2)" id="btn2" class="preset">🔒 Гонка</button>
            <button onclick="selectMap(3)" id="btn3" class="custom">✏️ Ручная</button>
        </div>
        <div class="warning" id="warning"></div>
    </div>

    <div class="card">
        <h2>Параметры двигателя</h2>
        <div id="live">Подключение...</div>
    </div>

    <div class="card">
        <h2 id="tableTitle">Редактирование карты</h2>
        <table id="mapTable"></table>
    </div>

    <div class="footer-buttons">
        <button onclick="applyMap()" id="applyBtn">Применить</button>
        <button onclick="saveAll()">Сохранить</button>
    </div>

    <script>
        let currentMap = 0;
        let isEditable = false;
        
        let mapsData = {
            0: [], 1: [], 2: [], 3: []
        };
        
        const rpmPoints = [1000,2000,3000,4000,5000,6000,7000,8000,9000,10000,11000,12000,13000,14000,15000];
        const mapNames = ['Нормальная', 'Спорт', 'Гонка', 'Ручная'];

        function updateLive() {
            fetch('/data')
                .then(r => r.json())
                .then(d => {
                    document.getElementById('live').innerHTML = 
                        `RPM: <b>${d.rpm}</b> | УОЗ: <b>${d.uoz}°</b><br>Карта: ${d.mapname}`;
                })
                .catch(() => {});
        }

        function loadMapTable() {
            let html = `<tr><th>RPM</th><th>УОЗ (°)</th></tr>`;
            for(let i = 0; i < 15; i++) {
                html += `<tr>
                    <td>${rpmPoints[i]}</td>
                    <td><input type="number" step="0.1" id="uoz${i}" value="12.0" disabled></td>
                </tr>`;
            }
            document.getElementById('mapTable').innerHTML = html;
        }

        function loadAllMaps() {
            fetch('/getmaps')
                .then(r => r.json())
                .then(data => {
                    mapsData[0] = data.normal;
                    mapsData[1] = data.sport;
                    mapsData[2] = data.race;
                    mapsData[3] = data.custom;
                    currentMap = data.current;
                    isEditable = data.editable;
                    
                    selectMap(currentMap);
                })
                .catch(() => {
                    setTimeout(loadAllMaps, 1000);
                });
        }

        function updateTableWithMap(mapIdx) {
            const map = mapsData[mapIdx];
            if (map.length > 0) {
                for(let i = 0; i < 15; i++) {
                    const input = document.getElementById('uoz' + i);
                    input.value = map[i];
                }
            }
        }

        function setEditMode(editable) {
            isEditable = editable;
            
            // Блокируем/разблокируем поля ввода
            for(let i = 0; i < 15; i++) {
                document.getElementById('uoz' + i).disabled = !editable;
            }
            
            // Меняем заголовок и предупреждение
            if (editable) {
                document.getElementById('tableTitle').innerHTML = '✏️ Редактирование РУЧНОЙ карты';
                document.getElementById('warning').innerHTML = '⚠️ Ручная настройка - можно редактировать!';
                document.getElementById('applyBtn').style.display = 'block';
            } else {
                document.getElementById('tableTitle').innerHTML = '🔒 Предустановленная карта (только чтение)';
                document.getElementById('warning').innerHTML = '🔒 Предустановленные карты нельзя изменить';
                document.getElementById('applyBtn').style.display = 'none';
            }
        }

        function selectMap(idx) {
            currentMap = idx;
            
            // Подсветка кнопок
            document.querySelectorAll('.map-buttons button').forEach((btn, i) => {
                btn.classList.toggle('active', i === idx);
            });
            
            // Обновляем таблицу
            if (mapsData[idx].length > 0) {
                updateTableWithMap(idx);
            }
            
            // Устанавливаем режим редактирования
            setEditMode(idx === 3);
        }

        function applyMap() {
            if (!isEditable) {
                alert('Предустановленные карты нельзя изменить!');
                return;
            }
            
            let fd = new FormData();
            fd.append("map", currentMap);
            for(let i = 0; i < 15; i++) {
                fd.append("uoz" + i, document.getElementById("uoz" + i).value);
            }
            fetch('/apply', {method: 'POST', body: fd})
                .then(() => {
                    alert('Ручная карта применена!');
                });
        }

        function saveAll() {
            let fd = new FormData();
            fd.append("map", currentMap);
            
            // Сохраняем текущие значения ручной карты
            for(let i = 0; i < 15; i++) {
                const val = document.getElementById('uoz' + i).value;
                mapsData[3][i] = parseFloat(val) || 12.0;
            }
            
            fetch('/save', {method: 'POST'})
                .then(() => alert('Настройки сохранены!'));
        }

        window.onload = () => {
            loadMapTable();
            loadAllMaps();
            setInterval(updateLive, 180);
        };
    </script>
</body>
</html>
)rawliteral";

#endif