#include <WiFi.h>
#include <WebServer.h> 
#include <Preferences.h>

// ==================== КОНСТАНТЫ ====================
const byte SENSOR_PIN = 17;
const byte IGNITION_PIN = 19;
const int SPARK_PULSE_US = 80;
const int DEBOUNCE_US = 50;  // ← УМЕНЬШЕНО ДЛЯ ПУСКА

// ==================== ДВЕ КАРТЫ УОЗ ====================
const float UOZ_MAP_STANDARD[] = {10, 12, 15, 18, 22, 26, 28, 27, 25, 22, 19, 16, 14, 12, 10};
const float UOZ_MAP_SPORT[]   = {12, 15, 18, 22, 26, 30, 32, 30, 28, 25, 22, 18, 15, 12, 10};

volatile int current_map = 0;

// ==================== ПЕРЕМЕННЫЕ ====================
volatile unsigned long spark_count = 0;
volatile unsigned long isr_count = 0;

volatile unsigned long full_cycle_buffer[8] = {0};
volatile byte full_buffer_idx = 0;
volatile unsigned long full_cycle_sum = 0;
volatile unsigned long full_cycle_avg = 100000;

volatile unsigned long last_240_time = 0;
volatile unsigned long last_vmt_time = 0;
volatile bool has_signal = false;
volatile unsigned long last_signal_time = 0;

volatile unsigned long calculated_delay = 0;
volatile float calculated_uoz = 10.0;
volatile bool need_calc = false;
volatile float current_rpm = 0;
volatile bool direct_spark_at_vmt = true;
volatile bool buffer_ready = false;

// ==================== ТАЙМЕР (esp_timer) ====================
esp_timer_handle_t pulse_timer = NULL;

WebServer server(80);
Preferences prefs;
TaskHandle_t IgnitionTaskHandle = NULL;
volatile bool system_ready = false;

// ==================== КОНЕЦ ИСКРЫ ====================
void IRAM_ATTR onPulseEnd(void* arg) {
    digitalWrite(IGNITION_PIN, HIGH);
}

// ==================== НАЧАТЬ ИСКРУ ====================
void IRAM_ATTR fireSpark() {
    spark_count++;
    digitalWrite(IGNITION_PIN, LOW);
    esp_timer_start_once(pulse_timer, SPARK_PULSE_US);
}

float getUOZ(float rpm) {
    const float* map = (current_map == 1) ? UOZ_MAP_SPORT : UOZ_MAP_STANDARD;
    
    if (rpm <= 1000) return map[0];
    if (rpm >= 15000) return map[14];
    
    int idx = (int)((rpm - 1000) / 1000);
    if (idx < 0) idx = 0;
    if (idx > 13) idx = 13;
    
    float frac = (rpm - (1000 + idx * 1000)) / 1000.0;
    return map[idx] + (map[idx+1] - map[idx]) * frac;
}

// ==================== ISR ДАТЧИКА ====================
void IRAM_ATTR sensorISR() {
    isr_count++;
    unsigned long now = micros();
    static unsigned long last_interrupt = 0;
    static bool last_state = HIGH;
    
    if (now - last_interrupt < DEBOUNCE_US) return;
    last_interrupt = now;
    
    bool state = digitalRead(SENSOR_PIN);
    if (state == last_state) return;
    last_state = state;
    
    if (state == HIGH) {
        // === 240° ===
        last_240_time = now;
        
        if (!direct_spark_at_vmt && calculated_delay >= 50 && calculated_delay <= 500000) {
            digitalWrite(IGNITION_PIN, HIGH);
            esp_timer_stop(pulse_timer);
            esp_timer_start_once(pulse_timer, calculated_delay);
        } else {
            esp_timer_stop(pulse_timer);
            digitalWrite(IGNITION_PIN, HIGH);
        }
        
    } else {
        // === ВМТ ===
        if (last_vmt_time > 0) {
            unsigned long full_cycle = now - last_vmt_time;
            
            if (full_cycle > 5000 && full_cycle < 2000000) {
                if (!buffer_ready) {
                    for (int i = 0; i < 8; i++) full_cycle_buffer[i] = full_cycle;
                    full_cycle_sum = full_cycle * 8;
                    full_buffer_idx = 0;   // ← ИСПРАВЛЕНО!
                    buffer_ready = true;
                } else {
                    unsigned long oldest = full_cycle_buffer[full_buffer_idx];
                    full_cycle_sum = full_cycle_sum - oldest + full_cycle;
                    full_cycle_buffer[full_buffer_idx] = full_cycle;
                    full_buffer_idx = (full_buffer_idx + 1) & 7;
                }
                full_cycle_avg = full_cycle_sum >> 3;
                has_signal = true;
                last_signal_time = millis();
                need_calc = true;
            }
        }
        last_vmt_time = now;
        
        if (direct_spark_at_vmt) {
            fireSpark();
        }
    }
}

// ==================== ЗАДАЧА РАСЧЕТА ====================
void IgnitionLoop(void * pvParameters) {
    pinMode(SENSOR_PIN, INPUT_PULLUP);
    pinMode(IGNITION_PIN, OUTPUT);
    digitalWrite(IGNITION_PIN, HIGH);
    
    // Таймер для длительности искры
    esp_timer_create_args_t pulse_args = {};
    pulse_args.callback = &onPulseEnd;
    pulse_args.name = "spark_pulse";
    esp_timer_create(&pulse_args, &pulse_timer);
    
    attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), sensorISR, CHANGE);
    
    for(;;) {
        // Таймаут сигнала (увеличен до 3 секунд для пуска)
        if (has_signal && (millis() - last_signal_time > 3000)) {
            has_signal = false;
            current_rpm = 0;
            direct_spark_at_vmt = true;
            calculated_delay = 0;
            buffer_ready = false;
            full_cycle_avg = 100000;
        }
        
        if (need_calc) {
            need_calc = false;
            
            unsigned long avg_cycle = full_cycle_avg;
            
            if (!has_signal || avg_cycle < 5000 || avg_cycle > 2000000) {
                current_rpm = 0;
                direct_spark_at_vmt = true;
                continue;
            }
            
            float rpm = 60000000.0 / (float)avg_cycle;
            
            if (rpm < 10 || rpm > 20000) {
                current_rpm = 0;
                direct_spark_at_vmt = true;
                continue;
            }
            
            current_rpm = rpm;
            unsigned long short_sector = avg_cycle / 3;
            
            if (rpm < 800) {
                direct_spark_at_vmt = true;
                calculated_delay = 0;
                calculated_uoz = 0;
            } else if (rpm < 1500) {
                direct_spark_at_vmt = false;
                calculated_uoz = 5.0 + (rpm - 800) * 7.0 / 700.0;
                calculated_delay = (unsigned long)(short_sector * (120.0 - calculated_uoz) / 120.0);
                if (calculated_delay < 50) calculated_delay = 50;
                if (calculated_delay > short_sector - 500) calculated_delay = short_sector - 500;
            } else {
                direct_spark_at_vmt = false;
                calculated_uoz = getUOZ(rpm);
                calculated_delay = (unsigned long)(short_sector * (120.0 - calculated_uoz) / 120.0);
                if (calculated_delay < 50) calculated_delay = 50;
                if (calculated_delay > short_sector - 500) calculated_delay = short_sector - 500;
            }
        }
        
        vTaskDelay(1);  // ← УМЕНЬШЕНО ДЛЯ БЫСТРОЙ РЕАКЦИИ
    }
}

// ==================== WEB ====================
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>CDI</title>
<style>
*{margin:0;padding:0}
body{font-family:Arial;background:#0a0a0a;color:#e0e0e0;display:flex;justify-content:center;align-items:center;min-height:100vh}
.c{text-align:center}
.r{font-size:80px;font-weight:bold;font-family:monospace;color:#00e676}
.g{display:grid;grid-template-columns:repeat(2,1fr);gap:15px;max-width:350px;margin:20px auto}
.i{background:#1a1a1a;padding:15px;border-radius:12px}
.l{font-size:10px;color:#888}
.v{font-size:20px;font-weight:bold;font-family:monospace;color:#00e676}
.m{font-size:16px;margin:10px;padding:8px 20px;border-radius:20px;display:inline-block}
.s{background:#ff9800;color:#000}
.t{background:#2196f3;color:#fff}
.w{background:#00e676;color:#000}
.btn{font-size:16px;padding:12px 30px;margin:10px;border:none;border-radius:25px;cursor:pointer;font-weight:bold}
.std{background:#00e676;color:#000}
.spt{background:#f44336;color:#fff}
.act{box-shadow:0 0 15px currentColor}
</style>
</head>
<body>
<div class="c">
<h1 style="color:#00e676">JetSurf CDI</h1>
<div class="r"><span id="rpm">0</span></div>
<div id="mode" class="m s">START</div>
<button id="bs" class="btn std act" onclick="m(0)">STANDARD</button>
<button id="bp" class="btn spt" onclick="m(1)">SPORT</button>
<div class="g">
<div class="i"><div class="l">UOZ</div><div class="v"><span id="uoz">0</span>°</div></div>
<div class="i"><div class="l">SPARKS</div><div class="v"><span id="sparks">0</span></div></div>
</div>
</div>
<script>
var sm=0;
function m(x){
fetch('/map?mode='+x).then(r=>r.text()).then(t=>{
if(t=='OK'){sm=x;
document.getElementById('bs').className='btn std'+(sm==0?' act':'');
document.getElementById('bp').className='btn spt'+(sm==1?' act':'');
}
});
}
function u(){
fetch('/data').then(r=>r.json()).then(d=>{
document.getElementById('rpm').innerText=Math.round(d.rpm);
document.getElementById('uoz').innerText=d.uoz.toFixed(1);
document.getElementById('sparks').innerText=d.sparks;
var e=document.getElementById('mode');
if(d.vmt){e.innerText='START';e.className='m s';}
else if(d.rpm<1500){e.innerText='IDLE '+d.uoz.toFixed(1)+'°';e.className='m t';}
else{e.innerText='RUN '+d.uoz.toFixed(1)+'°';e.className='m w';}
}).catch(e=>{});
}
setInterval(u,200);u();
</script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", PAGE); }
void handleData() {
    String json = "{\"rpm\":" + String(current_rpm,0) + 
                  ",\"uoz\":" + String(calculated_uoz,1) +
                  ",\"sparks\":" + String(spark_count) +
                  ",\"vmt\":" + String(direct_spark_at_vmt?1:0) + "}";
    server.send(200, "application/json", json);
}
void handleMap() {
    if (server.hasArg("mode")) {
        current_map = server.arg("mode").toInt();
        prefs.begin("cdi", false);
        prefs.putInt("map_mode", current_map);
        prefs.end();
        server.send(200, "text/plain", "OK");
    }
}

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    delay(500);
    
    prefs.begin("cdi", false);
    current_map = prefs.getInt("map_mode", 0);
    prefs.end();
    
    // Запуск задачи зажигания на ЯДРЕ 0 (чтобы не мешать Wi-Fi)
    xTaskCreatePinnedToCore(IgnitionLoop, "Ign", 8192, NULL, configMAX_PRIORITIES-1, &IgnitionTaskHandle, 0);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP("JetSurf_CDI", "12345678");
    server.on("/", handleRoot);
    server.on("/data", handleData);
    server.on("/map", handleMap);
    server.begin();
    
    system_ready = true;
    Serial.println("JetSurf CDI Ready");
    Serial.println("SSID: JetSurf_CDI");
    Serial.println("IP: 192.168.4.1");
}

// ==================== LOOP ====================
void loop() {
    server.handleClient();
    delay(10);
}