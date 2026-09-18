#include <WiFi.h>
#include <WebServer.h> 
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <driver/gpio.h>
#include <driver/timer.h>
#include <Update.h>
#include "web_page.h"

// Отключение Bluetooth
#include "esp_bt.h"
#include "esp_bt_main.h"

// ==================== КОНСТАНТЫ ====================
const byte SENSOR_PIN = 17;   
const byte IGNITION_PIN = 19;

const int MAP_SIZE = 15;
const int RPM_STEP = 1000;
const int RPM_START = 1000;

const int SPARK_PULSE_US = 80;
const int MIN_ISR_INTERVAL_US = 10;

// Коррекция установки датчика
const float SENSOR_OFFSET_MIN = -3.0;
const float SENSOR_OFFSET_MAX = 3.0;
const float SENSOR_OFFSET_STEP = 0.1;

// ==================== АППАРАТНЫЙ ТАЙМЕР ====================
#define TIMER_DIVIDER         80
hw_timer_t * spark_timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
volatile bool spark_fired = false;

// ==================== СТРУКТУРЫ ====================
struct MapPoint {
  int rpm;
  float uoz;
};
MapPoint uoz_map[MAP_SIZE];

static portMUX_TYPE ignitionMux = portMUX_INITIALIZER_UNLOCKED;

// ==================== ПЕРЕМЕННЫЕ ЗАЖИГАНИЯ ====================
volatile unsigned long short_sector_buffer[16] = {0};
volatile byte short_buffer_idx = 0;
volatile unsigned long short_sector_sum = 0;
volatile unsigned long short_sector_avg = 0;

volatile unsigned long last_240_time = 0;
volatile unsigned long last_vmt_time = 0;

volatile bool has_valid_signal = false;
volatile unsigned long last_signal_time = 0;

volatile unsigned long calculated_delay = 0;
volatile float calculated_uoz = 12.0;
volatile bool need_calculation = false;

volatile float current_rpm = 0;
volatile int max_rpm_limit = 16000;
volatile int soft_rpm_limit = 15000;
volatile bool hard_cut_active = false;
volatile bool direct_spark_at_vmt = true;

// Коррекция установки датчика
volatile float sensor_offset_angle = 0.0;

// Для инициализации буфера
volatile bool buffer_initialized = false;

// ==================== Wi-Fi УПРАВЛЕНИЕ ====================
volatile bool wifi_enabled = true;
volatile int wifi_off_rpm = 3000;
unsigned long wifi_debounce_timer = 0;
volatile bool engine_was_running = false;

// ==================== РЕЖИМЫ ====================
enum EngineMode { MODE_START, MODE_NORMAL, MODE_SOFT_CUT, MODE_HARD_CUT, MODE_DRY };
const char* mode_names[] = { "START", "NORMAL", "SOFT_LIMIT", "HARD_LIMIT", "DRY" };
volatile EngineMode current_mode = MODE_START;

// ==================== ПРОСУШКА СВЕЧИ ====================
volatile bool dry_mode = false;
volatile unsigned long dry_start_time = 0;
const unsigned long DRY_DURATION = 5000;
const int DRY_RPM = 800;

// ==================== ОСТАЛЬНЫЕ ПЕРЕМЕННЫЕ ====================
WebServer server(80); 
Preferences prefs;
TaskHandle_t IgnitionTaskHandle = NULL;

float last_cached_rpm = -1.0;
float last_cached_uoz = 0.0;

bool ota_in_progress = false;
volatile bool system_ready = false;

// ==================== ФУНКЦИЯ ПОДАВЛЕНИЯ ШУМОВ ====================
void setupPinsForNoiseReduction() {
    pinMode(SENSOR_PIN, INPUT_PULLUP);
    pinMode(IGNITION_PIN, OUTPUT);
    digitalWrite(IGNITION_PIN, HIGH);
    
    // Заземляем неиспользуемые пины для снижения наводок
    for (int i = 0; i <= 39; i++) {
        if (i != SENSOR_PIN && i != IGNITION_PIN && 
            i != 1 && i != 3 &&  // TX/RX
            i != 21 && i != 22) {  // I2C
            pinMode(i, INPUT_PULLDOWN);
        }
    }
}

// ==================== ФУНКЦИИ АППАРАТНОГО ТАЙМЕРА ====================
void IRAM_ATTR onSparkTimer() {
    portENTER_CRITICAL_ISR(&timerMux);
    
    if (!spark_fired) {
        // Начало искры
        GPIO.out_w1tc = (1 << IGNITION_PIN);  // LOW
        spark_fired = true;
        
        // Таймер на длительность искры
        timerAlarmWrite(spark_timer, SPARK_PULSE_US, false);
        timerAlarmEnable(spark_timer);
    } else {
        // Конец искры
        GPIO.out_w1ts = (1 << IGNITION_PIN);  // HIGH
        spark_fired = false;
        timerAlarmDisable(spark_timer);
    }
    
    portEXIT_CRITICAL_ISR(&timerMux);
}

void IRAM_ATTR scheduleSpark(unsigned long delay_us) {
    if (delay_us == 0 || delay_us > 1000000) return;
    
    portENTER_CRITICAL_ISR(&timerMux);
    
    spark_fired = false;
    timerAlarmDisable(spark_timer);
    timerAlarmWrite(spark_timer, delay_us, false);
    timerAlarmEnable(spark_timer);
    
    portEXIT_CRITICAL_ISR(&timerMux);
}

void initSparkTimer() {
    spark_timer = timerBegin(0, TIMER_DIVIDER, true);
    timerAttachInterrupt(spark_timer, &onSparkTimer, true);
    timerAlarmWrite(spark_timer, 0, false);
    timerAlarmDisable(spark_timer);
    
    Serial.println("Hardware timer initialized (1 MHz, 1 µs resolution)");
}

// ==================== ФУНКЦИЯ ИНТЕРПОЛЯЦИИ УОЗ ====================
float getInterpolatedUOZ(float rpm) {
    if (fabs(rpm - last_cached_rpm) < 10.0) {
        return last_cached_uoz; 
    }
    
    float result = 10.0;
    
    if (rpm <= uoz_map[0].rpm) {
        result = uoz_map[0].uoz;
    } else if (rpm >= uoz_map[MAP_SIZE - 1].rpm) {
        result = uoz_map[MAP_SIZE - 1].uoz;
    } else {
        for (int i = 0; i < MAP_SIZE - 1; i++) {
            if (rpm >= uoz_map[i].rpm && rpm <= uoz_map[i+1].rpm) {
                float range = uoz_map[i+1].rpm - uoz_map[i].rpm;
                if (range > 0.1) {
                    result = uoz_map[i].uoz + (rpm - uoz_map[i].rpm) * 
                             (uoz_map[i+1].uoz - uoz_map[i].uoz) / range;
                }
                break;
            }
        }
    }
    
    // Применяем коррекцию установки датчика
    result += sensor_offset_angle;
    
    // Ограничения безопасности
    if (result < 0) result = 0;
    if (result > 110) result = 110;
    
    last_cached_rpm = rpm;
    last_cached_uoz = result;
    return result;
}

// ==================== ISR ДАТЧИКА ====================
void IRAM_ATTR sensorISR(void* arg) {
    unsigned long now = micros();
    static unsigned long last_isr_time = 0;
    static bool last_pin_state = HIGH;
    
    // Программный антидребезг с проверкой изменения состояния
    bool pin_state = gpio_get_level((gpio_num_t)SENSOR_PIN);
    
    // Проверяем минимальный интервал между прерываниями
    if (now - last_isr_time < MIN_ISR_INTERVAL_US) {
        return;
    }
    
    // Проверяем, действительно ли изменилось состояние
    if (pin_state == last_pin_state) {
        last_isr_time = now;
        return;
    }
    
    last_isr_time = now;
    last_pin_state = pin_state;
    
    if (pin_state == HIGH) {
        // === 240° ===
        last_240_time = now;
        
        // Останавливаем таймер при новом цикле
        timerAlarmDisable(spark_timer);
        GPIO.out_w1ts = (1 << IGNITION_PIN);  // HIGH
        
        // Планируем искру с учетом коррекции
        if (!direct_spark_at_vmt && calculated_delay > 0 && 
            calculated_delay < 500000 && !hard_cut_active && 
            !dry_mode && system_ready) {
            scheduleSpark(calculated_delay);
        }
        
    } else if (pin_state == LOW) {
        // === ВМТ ===
        if (last_240_time > 0) {
            unsigned long short_sector = now - last_240_time;
            
            if (short_sector > 5000 && short_sector < 2000000) {
                portENTER_CRITICAL_ISR(&ignitionMux);
                
                if (!buffer_initialized) {
                    for (int i = 0; i < 16; i++) {
                        short_sector_buffer[i] = short_sector;
                    }
                    short_sector_sum = short_sector * 16;
                    buffer_initialized = true;
                } else {
                    unsigned long oldest = short_sector_buffer[short_buffer_idx];
                    short_sector_sum = short_sector_sum - oldest + short_sector;
                    short_sector_buffer[short_buffer_idx] = short_sector;
                }
                
                short_buffer_idx = (short_buffer_idx + 1) & 15;
                short_sector_avg = short_sector_sum >> 4;
                
                portEXIT_CRITICAL_ISR(&ignitionMux);
                
                has_valid_signal = true;
                last_signal_time = millis();
                need_calculation = true;
            }
        }
        
        last_vmt_time = now;
        
        // Прямая искра в ВМТ
        if (direct_spark_at_vmt && !hard_cut_active && 
            !dry_mode && system_ready) {
            timerAlarmDisable(spark_timer);
            GPIO.out_w1tc = (1 << IGNITION_PIN);  // LOW
            spark_fired = true;
            timerAlarmWrite(spark_timer, SPARK_PULSE_US, false);
            timerAlarmEnable(spark_timer);
        }
    }
}

// ==================== ЗАДАЧА ЗАЖИГАНИЯ ====================
void IgnitionLoop(void * pvParameters) {
    // Настройка пина датчика
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << SENSOR_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    pinMode(IGNITION_PIN, OUTPUT);
    GPIO.out_w1ts = (1 << IGNITION_PIN);  // HIGH
    
    initSparkTimer();
    
    esp_task_wdt_add(NULL); 
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add((gpio_num_t)SENSOR_PIN, sensorISR, NULL);
    system_ready = true;
    
    Serial.println("Ignition system ready");
    Serial.printf("Sensor offset: %.1f degrees\n", sensor_offset_angle);
    
    unsigned long last_dry_spark = 0;
    const unsigned long DRY_INTERVAL = 25000;
    
    for(;;) {
        esp_task_wdt_reset(); 
        
        // ===== ПРОСУШКА СВЕЧИ =====
        if (dry_mode) {
            if (millis() - dry_start_time > DRY_DURATION) {
                dry_mode = false;
                current_mode = MODE_START;
                direct_spark_at_vmt = true;
                Serial.println("Dry mode completed");
            } else {
                current_mode = MODE_DRY;
                current_rpm = DRY_RPM;
                direct_spark_at_vmt = false;
                
                unsigned long now_us = micros();
                if (now_us - last_dry_spark >= DRY_INTERVAL) {
                    last_dry_spark = now_us;
                    float target_uoz = getInterpolatedUOZ(DRY_RPM);
                    unsigned long delay_us = (unsigned long)((float)DRY_INTERVAL * (120.0 - target_uoz) / 120.0);
                    
                    if (delay_us < 10) delay_us = 10;
                    if (delay_us > DRY_INTERVAL - 100) delay_us = DRY_INTERVAL - 100;
                    
                    if (!hard_cut_active && system_ready) {
                        scheduleSpark(delay_us);
                    }
                }
                vTaskDelay(1 / portTICK_PERIOD_MS);
                continue;
            }
        }
        
        // ===== ТАЙМАУТ СИГНАЛА =====
        if (!dry_mode && has_valid_signal && (millis() - last_signal_time > 1000)) {
            has_valid_signal = false;
            current_rpm = 0;
            calculated_delay = 0;
            current_mode = MODE_START;
            direct_spark_at_vmt = true;
            buffer_initialized = false;
            
            portENTER_CRITICAL(&ignitionMux);
            short_sector_avg = 0;
            short_sector_sum = 0;
            for (int i = 0; i < 16; i++) {
                short_sector_buffer[i] = 0;
            }
            portEXIT_CRITICAL(&ignitionMux);
            
            Serial.println("Signal lost");
        }
        
        // ===== РАСЧЕТ RPM И УОЗ =====
        if (need_calculation) {
            need_calculation = false;
            
            portENTER_CRITICAL(&ignitionMux);
            unsigned long avg_short = short_sector_avg;
            portEXIT_CRITICAL(&ignitionMux);
            
            if (!has_valid_signal || avg_short == 0 || avg_short < 5000 || avg_short > 2000000) {
                current_rpm = 0;
                current_mode = MODE_START;
                calculated_delay = 0;
                direct_spark_at_vmt = true;
                continue;
            }
            
            float rpm = 20000000.0 / (float)avg_short;
            
            if (rpm > 20000) {
                current_rpm = 0;
                current_mode = MODE_START;
                calculated_delay = 0;
                direct_spark_at_vmt = true;
                continue;
            }
            
            current_rpm = rpm;
            
            // Запуск: прямая искра до 200 RPM
            if (rpm < 200) {
                direct_spark_at_vmt = true;
                current_mode = MODE_START;
                calculated_delay = 0;
                calculated_uoz = 0;
                continue;
            }
            
            direct_spark_at_vmt = false;
            engine_was_running = true;
            
            // ===== РАСЧЕТ УОЗ С КОРРЕКЦИЕЙ =====
            float target_uoz = getInterpolatedUOZ(rpm);
            hard_cut_active = false;
            
            // Мягкая отсечка
            if (rpm >= soft_rpm_limit && rpm < max_rpm_limit) {
                current_mode = MODE_SOFT_CUT;
                float overshoot = rpm - soft_rpm_limit;
                float window = max_rpm_limit - soft_rpm_limit;
                if (window < 50) window = 50;
                
                float reduction = overshoot * 30.0 / window;
                if (reduction > 30) reduction = 30;
                
                target_uoz = getInterpolatedUOZ(rpm) - reduction;
                if (target_uoz < 0) target_uoz = 0;
            }
            // Жесткая отсечка
            else if (rpm >= max_rpm_limit) {
                current_mode = MODE_HARD_CUT;
                hard_cut_active = true;
                calculated_delay = 0;
                calculated_uoz = 0;
                continue;
            }
            else {
                current_mode = MODE_NORMAL;
            }
            
            if (!dry_mode && !hard_cut_active) {
                if (target_uoz > 110) target_uoz = 110;
                if (target_uoz < 0) target_uoz = 0;
                
                calculated_uoz = target_uoz;
                
                // Расчет задержки с коррекцией датчика
                unsigned long delay_us = (unsigned long)((float)avg_short * (120.0 - target_uoz) / 120.0);
                
                if (delay_us < 20) delay_us = 20;
                if (delay_us > avg_short - 200) delay_us = avg_short - 200;
                
                calculated_delay = delay_us;
            }
        }
        
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

// ==================== WEB HANDLERS ====================

void handleRoot() {
    String html = String(INDEX_HTML);
    html.replace("%MAX_RPM%", String(max_rpm_limit));
    html.replace("%SOFT_RPM%", String(soft_rpm_limit));
    html.replace("%SENSOR_OFFSET%", String(sensor_offset_angle, 1));
    
    String json = "[";
    for (int i = 0; i < MAP_SIZE; i++) {
        if (i > 0) json += ",";
        json += "{\"rpm\":" + String(uoz_map[i].rpm);
        json += ",\"uoz\":" + String(uoz_map[i].uoz, 1);
        json += "}";
    }
    json += "]";
    html.replace("%MAP_DATA%", json);
    
    server.send(200, "text/html", html);
}

void handleApply() {
    if(server.hasArg("max_rpm")) {
        int val = server.arg("max_rpm").toInt();
        if (val >= 5000 && val <= 18000) max_rpm_limit = val;
    }
    if(server.hasArg("soft_rpm")) {
        int val = server.arg("soft_rpm").toInt();
        if (val >= 4000 && val <= 18000) soft_rpm_limit = val;
    }
    if(server.hasArg("sensor_offset")) {
        float val = server.arg("sensor_offset").toFloat();
        if (val >= SENSOR_OFFSET_MIN && val <= SENSOR_OFFSET_MAX) {
            sensor_offset_angle = val;
            last_cached_rpm = -1.0;
            Serial.printf("Sensor offset updated: %.1f°\n", sensor_offset_angle);
        }
    }
    for (int i = 0; i < MAP_SIZE; i++) {
        String pName = "uoz" + String(i);
        if (server.hasArg(pName)) {
            float val = server.arg(pName).toFloat();
            if (val >= 0 && val <= 35) uoz_map[i].uoz = val;
        }
    }
    last_cached_rpm = -1.0;
    server.send(200, "text/plain", "OK");
}

void handleSave() {
    prefs.begin("ignition", false);
    prefs.putBool("map_saved", true);
    prefs.putInt("max_rpm", max_rpm_limit);
    prefs.putInt("soft_rpm", soft_rpm_limit);
    prefs.putInt("wifi_off_rpm", wifi_off_rpm);
    prefs.putFloat("sensor_offset", sensor_offset_angle);
    
    for (int i = 0; i < MAP_SIZE; i++) {
        prefs.putFloat(("uoz" + String(i)).c_str(), uoz_map[i].uoz);
    }
    prefs.end();
    
    Serial.println("Settings saved to NVS");
    Serial.printf("Sensor offset saved: %.1f°\n", sensor_offset_angle);
    server.send(200, "text/plain", "Saved successfully");
}

void handleData() {
    String json = "{\"rpm\":" + String(current_rpm, 0) + 
                  ",\"mode\":\"" + String(mode_names[current_mode]) + "\"" +
                  ",\"uoz\":" + String(calculated_uoz, 1) +
                  ",\"delay\":" + String(calculated_delay) +
                  ",\"offset\":" + String(sensor_offset_angle, 1) +
                  ",\"wifi\":" + String(wifi_enabled ? 1 : 0) +
                  "}";
    server.send(200, "application/json", json);
}

void handleWifiSettings() {
    if (server.hasArg("wifi_off_rpm")) {
        int val = server.arg("wifi_off_rpm").toInt();
        if (val >= 0 && val <= 16000) {
            wifi_off_rpm = val;
            prefs.begin("ignition", false);
            prefs.putInt("wifi_off_rpm", wifi_off_rpm);
            prefs.end();
            server.send(200, "text/plain", "OK");
            return;
        }
    }
    server.send(400, "text/plain", "Bad request");
}

void handleDryMode() {
    if (!dry_mode) {
        dry_mode = true;
        dry_start_time = millis();
        server.send(200, "text/plain", "Dry mode started for " + String(DRY_DURATION/1000) + "s");
    } else {
        server.send(200, "text/plain", "Dry mode already active");
    }
}

void handleDryStatus() {
    String json = "{\"active\":" + String(dry_mode ? 1 : 0) + 
                  ",\"remaining\":" + String(dry_mode ? (DRY_DURATION - (millis() - dry_start_time)) / 1000 : 0) + "}";
    server.send(200, "application/json", json);
}

void handleMap() {
    String json = "[";
    for (int i = 0; i < MAP_SIZE; i++) {
        if (i > 0) json += ",";
        json += "{\"rpm\":" + String(uoz_map[i].rpm);
        json += ",\"uoz\":" + String(uoz_map[i].uoz, 1);
        json += "}";
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleLimits() {
    String json = "{\"hard\":" + String(max_rpm_limit) + 
                  ",\"soft\":" + String(soft_rpm_limit) + 
                  ",\"offset\":" + String(sensor_offset_angle, 1) + "}";
    server.send(200, "application/json", json);
}

void handleSensorOffset() {
    if (server.hasArg("value")) {
        float val = server.arg("value").toFloat();
        if (val >= SENSOR_OFFSET_MIN && val <= SENSOR_OFFSET_MAX) {
            sensor_offset_angle = val;
            last_cached_rpm = -1.0;
            
            prefs.begin("ignition", false);
            prefs.putFloat("sensor_offset", sensor_offset_angle);
            prefs.end();
            
            Serial.printf("Sensor offset set to: %.1f°\n", sensor_offset_angle);
            server.send(200, "application/json", 
                       "{\"status\":\"ok\",\"offset\":" + String(sensor_offset_angle, 1) + "}");
            return;
        }
    }
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid value\"}");
}

void handleOTA() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>OTA Update</title>
<style>
body{font-family:sans-serif;background:#0a0a0a;color:#fff;padding:20px;text-align:center}
.container{max-width:500px;margin:0 auto;background:#1a1a1a;padding:30px;border-radius:20px}
.btn{background:#00e676;color:#000;padding:12px 24px;border:none;border-radius:30px;cursor:pointer;margin:10px;font-size:16px}
input{margin:20px 0;padding:10px}
#status{margin-top:20px;padding:10px}
</style>
</head>
<body>
<div class="container">
<h1>OTA Firmware Update</h1>
<p>Select .bin file and click Update</p>
<input type="file" id="file" accept=".bin"><br>
<button class="btn" onclick="startUpdate()">Update Firmware</button>
<button class="btn" onclick="location.href='/'">Back</button>
<div id="status"></div>
</div>
<script>
function startUpdate(){
let f=document.getElementById('file').files[0];
if(!f){alert('Select file first!');return;}
let d=new FormData();
d.append('firmware',f);
document.getElementById('status').innerHTML='<span style=color:yellow>Uploading...</span>';
fetch('/update',{method:'POST',body:d})
.then(r=>r.text())
.then(t=>{
if(t==='OK'){
document.getElementById('status').innerHTML='<span style=color:green>Success! Rebooting...</span>';
setTimeout(()=>{window.location.href='/'},3000);
}else{
document.getElementById('status').innerHTML='<span style=color:red>Error: '+t+'</span>';
}
})
.catch(e=>document.getElementById('status').innerHTML='<span style=color:red>Network error</span>');
}
</script>
</body>
</html>
)rawliteral";
    server.send(200, "text/html", html);
}

void handleUpdate() {
    if (ota_in_progress) {
        server.send(503, "text/plain", "Update in progress");
        return;
    }
    
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        ota_in_progress = true;
        Serial.println("OTA: Starting update...");
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
            ota_in_progress = false;
        }
        gpio_isr_handler_remove((gpio_num_t)SENSOR_PIN);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.println("OTA: Success!");
            server.send(200, "text/plain", "OK");
            delay(1000);
            ESP.restart();
        } else {
            server.send(500, "text/plain", "Update failed");
        }
        ota_in_progress = false;
    }
}

// ==================== SETUP ====================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=================================");
    Serial.println("JetSurf Smart CDI v2.2");
    Serial.println("With sensor offset correction");
    Serial.println("=================================\n");
    
    // Инициализация буфера
    for (int i = 0; i < 16; i++) {
        short_sector_buffer[i] = 0;
    }
    short_sector_sum = 0;
    short_sector_avg = 0;
    
    // === РАБОЧАЯ КАРТА УОЗ ===
    float working_map[] = {20, 22, 24, 26, 28, 30, 30, 30, 28, 26, 24, 22, 20, 18, 16};
    for(int i = 0; i < MAP_SIZE; i++) {
        uoz_map[i].rpm = RPM_START + (i * RPM_STEP);
        uoz_map[i].uoz = working_map[i];
    }
    
    // Отключаем Bluetooth
    btStop();
    delay(100);
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        esp_bt_controller_disable();
        delay(100);
    }
    esp_bt_controller_deinit();
    
    setupPinsForNoiseReduction();
    
    // Wi-Fi в режиме точки доступа
    WiFi.mode(WIFI_AP);
    WiFi.softAP("JetSurf_Smart_CDI", "12345678");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    
    // Загрузка сохраненных настроек
    prefs.begin("ignition", false);
    max_rpm_limit = prefs.getInt("max_rpm", 16000);
    soft_rpm_limit = prefs.getInt("soft_rpm", 15000);
    wifi_off_rpm = prefs.getInt("wifi_off_rpm", 3000);
    sensor_offset_angle = prefs.getFloat("sensor_offset", 0.0);
    
    if (prefs.isKey("map_saved")) {
        for (int i = 0; i < MAP_SIZE; i++) {
            float saved = prefs.getFloat(("uoz" + String(i)).c_str(), uoz_map[i].uoz);
            if (saved > 0 && saved < 35) uoz_map[i].uoz = saved;
        }
    }
    prefs.end();
    
    Serial.printf("Loaded sensor offset: %.1f°\n", sensor_offset_angle);
    
    // Настройка веб-сервера
    server.on("/", handleRoot);
    server.on("/apply", HTTP_POST, handleApply);
    server.on("/save", handleSave);
    server.on("/data", handleData);
    server.on("/map", handleMap);
    server.on("/limits", handleLimits);
    server.on("/dry", HTTP_GET, handleDryMode);
    server.on("/dry/status", HTTP_GET, handleDryStatus);
    server.on("/offset", HTTP_POST, handleSensorOffset);
    server.on("/ota", HTTP_GET, handleOTA);
    server.on("/update", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleUpdate);
    server.on("/wifi", HTTP_POST, handleWifiSettings);
    server.begin();
    
    ArduinoOTA.setHostname("JetSurf-CDI");
    ArduinoOTA.begin();
    
    esp_task_wdt_init(5, true);
    
    xTaskCreatePinnedToCore(
        IgnitionLoop, 
        "IgnitionTask", 
        8192, 
        NULL, 
        configMAX_PRIORITIES - 1, 
        &IgnitionTaskHandle, 
        1
    );
    
    Serial.println("\n=================================");
    Serial.println("System Ready!");
    Serial.println("Connect to: JetSurf_Smart_CDI");
    Serial.println("Password: 12345678");
    Serial.println("Web interface: http://192.168.4.1");
    Serial.println("=================================\n");
}

// ==================== LOOP ====================

void loop() {
    ArduinoOTA.handle();
    server.handleClient();
    
    // Управление Wi-Fi по оборотам
    static unsigned long last_check = 0;
    if (millis() - last_check > 200) {
        last_check = millis();
        
        float r = current_rpm;
        
        if (r > wifi_off_rpm && wifi_off_rpm > 0 && engine_was_running) {
            if (wifi_enabled) {
                if (wifi_debounce_timer == 0) {
                    wifi_debounce_timer = millis();
                } else if (millis() - wifi_debounce_timer > 2000) {
                    WiFi.softAPdisconnect(true);
                    WiFi.mode(WIFI_OFF);
                    esp_wifi_stop();
                    wifi_enabled = false;
                    wifi_debounce_timer = 0;
                    Serial.println("Wi-Fi OFF: Racing mode");
                }
            }
        } else {
            wifi_debounce_timer = 0;
            if (!wifi_enabled && r < 50 && !has_valid_signal) {
                esp_wifi_start();
                WiFi.mode(WIFI_AP);
                WiFi.softAP("JetSurf_Smart_CDI", "12345678");
                wifi_enabled = true;
                server.begin();
                engine_was_running = false;
                Serial.println("Wi-Fi ON: Tuning mode");
            }
        }
    }
    
    // Отладка
    static unsigned long last_debug = 0;
    if (millis() - last_debug > 500) {
        last_debug = millis();
        Serial.printf("RPM: %4.0f | Mode: %-10s | UOZ: %5.1f° | Offset: %+.1f° | Signal: %s\n",
                     current_rpm,
                     mode_names[current_mode],
                     calculated_uoz,
                     sensor_offset_angle,
                     has_valid_signal ? "YES" : "NO");
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
}