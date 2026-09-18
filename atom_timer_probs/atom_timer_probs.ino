#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <driver/gpio.h>
#include <driver/timer.h>
#include "web_page.h"

#include "esp_bt.h"
#include "esp_bt_main.h"

// ==================== КОНСТАНТЫ ====================
const byte SENSOR_PIN = 17;
const byte IGNITION_PIN = 19;
const int SPARK_PULSE_US = 80;

#define RPM_TIMER_IDX      TIMER_0
#define SPARK_TIMER_IDX    TIMER_1
#define TIMER_DIVIDER      80

const int SPARK_VMT_MAX = 4000;
const int ADVANCE_MIN = 3000;

// ==================== ПЕРЕМЕННЫЕ ====================
volatile bool signal_vmt_core1 = false;
volatile bool signal_vmt_core0 = false;

volatile unsigned long signal_timestamp = 0;
volatile uint64_t sector_avg = 20000;

volatile bool system_ready = false;

float raw_rpm = 0;
float displayed_rpm = 0;
float target_uoz = 12.0;
volatile unsigned long calculated_delay_us = 0;

volatile uint64_t sector_buffer[8] = {20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000};
volatile uint8_t buffer_idx = 0;
volatile uint64_t sector_sum = 160000;

volatile uint8_t spark_phase = 0;

hw_timer_t *rpm_timer = NULL;
hw_timer_t *spark_timer = NULL;

WebServer server(80);
Preferences prefs;

const int MAP_SIZE = 15;
const int MAP_COUNT = 4;
int rpm_points[MAP_SIZE] = {1000,2000,3000,4000,5000,6000,7000,8000,9000,10000,11000,12000,13000,14000,15000};

// ==================== КАРТЫ УОЗ ====================
const float uoz_normal[MAP_SIZE] = {1, 1, 12, 22, 24, 26, 29, 31, 32, 32, 31, 29, 26, 22, 18};
const float uoz_sport[MAP_SIZE]  = {1, 1, 12, 24, 26, 30, 33, 35, 36, 36, 35, 33, 30, 26, 22};
const float uoz_race[MAP_SIZE]   = {1, 1, 12, 28, 30, 34, 37, 39, 40, 40, 39, 37, 34, 30, 26};
float uoz_custom[MAP_SIZE] = {1, 1, 1, 22, 24, 26, 29, 31, 32, 32, 31, 29, 26, 22, 18};

const float* current_map = uoz_normal;
int current_map_index = 0;
const char* map_names[MAP_COUNT] = {"Нормальная", "Спорт", "Гонка", "Ручная"};

// ==================== ISR датчика ====================
void IRAM_ATTR sensorISR(void* arg) {
    if (!system_ready) return;

    static unsigned long last_interrupt = 0;
    static bool last_level = 0;

    unsigned long now = micros();
    if (now - last_interrupt < 100) return;

    int level = gpio_get_level((gpio_num_t)SENSOR_PIN);
    if (level == last_level) return;
    last_level = level;
    last_interrupt = now;

    if (level == 1) {
        // ===== 240° - ЗАПУСК ОБОИХ ТАЙМЕРОВ =====
        
        // 1. Запуск RPM-таймера (измеряет сектор 120° до ВМТ)
        timerStop(rpm_timer);
        timerWrite(rpm_timer, 0);
        timerStart(rpm_timer);
        
        // 2. Запуск Spark-таймера (задержка опережения)
        if (raw_rpm >= ADVANCE_MIN && calculated_delay_us > 50 && spark_phase == 0) {
            timerStop(spark_timer);
            timerWrite(spark_timer, 0);
            timerAlarmWrite(spark_timer, calculated_delay_us, false);
            timerAlarmEnable(spark_timer);
            timerStart(spark_timer);
        }
        
    } else {
        // ===== ВМТ - ЧИТАЕМ RPM-ТАЙМЕР =====
        signal_vmt_core1 = true;
        signal_vmt_core0 = true;
        signal_timestamp = now;

        // Читаем время сектора 120° (от 240° до ВМТ)
        uint64_t sector_120 = timerRead(rpm_timer);
        timerStop(rpm_timer);
        timerWrite(rpm_timer, 0);
        
        if (sector_120 > 2000 && sector_120 < 500000) {
            uint64_t oldest = sector_buffer[buffer_idx];
            sector_sum = sector_sum - oldest + sector_120;
            sector_buffer[buffer_idx] = sector_120;
            buffer_idx = (buffer_idx + 1) & 7;
            sector_avg = sector_sum >> 3;
        }
    }
}

// ==================== ISR таймера опережения ====================
void IRAM_ATTR onSparkTimer() {
    if (spark_phase == 0) {
        GPIO.out_w1tc = (1 << IGNITION_PIN);
        spark_phase = 1;
        
        timerAlarmWrite(spark_timer, SPARK_PULSE_US, false);
        timerAlarmEnable(spark_timer);
        timerStart(spark_timer);
        
    } else {
        GPIO.out_w1ts = (1 << IGNITION_PIN);
        spark_phase = 0;
        
        timerStop(spark_timer);
        timerWrite(spark_timer, 0);
    }
}

// ==================== ФУНКЦИИ ====================
float getUOZ(float rpm) {
    if (rpm <= rpm_points[0]) return current_map[0];
    if (rpm >= rpm_points[MAP_SIZE-1]) return current_map[MAP_SIZE-1];
    
    for (int i = 0; i < MAP_SIZE-1; i++) {
        if (rpm >= rpm_points[i] && rpm <= rpm_points[i+1]) {
            float ratio = (rpm - rpm_points[i]) / (float)(rpm_points[i+1] - rpm_points[i]);
            return current_map[i] + (current_map[i+1] - current_map[i]) * ratio;
        }
    }
    return 12.0;
}

void vmtSpark() {
    GPIO.out_w1tc = (1 << IGNITION_PIN);
    delayMicroseconds(SPARK_PULSE_US);
    GPIO.out_w1ts = (1 << IGNITION_PIN);
}

void setCurrentMap(int idx) {
    if (idx < 0 || idx >= MAP_COUNT) idx = 0;
    current_map_index = idx;
    switch(idx) {
        case 0: current_map = uoz_normal; break;
        case 1: current_map = uoz_sport; break;
        case 2: current_map = uoz_race; break;
        case 3: current_map = uoz_custom; break;
        default: current_map = uoz_normal; break;
    }
}

// ==================== ЯДРО 1: РАСЧЁТ ПОСЛЕ ВМТ ====================
void IgnitionLoop(void * pvParameters) {
    // GPIO настраиваем здесь
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << SENSOR_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_config_t out_conf = {};
    out_conf.pin_bit_mask = (1ULL << IGNITION_PIN);
    out_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&out_conf);
    GPIO.out_w1ts = (1 << IGNITION_PIN);

    // Таймеры уже созданы в setup(), только настраиваем ISR датчика
    gpio_install_isr_service(0);
    gpio_isr_handler_add((gpio_num_t)SENSOR_PIN, sensorISR, NULL);

    system_ready = true;
    Serial.println("\n=== CDI: Dual Timer ===\n");

    for(;;) {
        while (!signal_vmt_core1) {
            vTaskDelay(1);
        }
        signal_vmt_core1 = false;
        
        if (sector_avg > 0) {
            raw_rpm = 60000000.0 / (float)(sector_avg * 3);
            if (displayed_rpm == 0) displayed_rpm = raw_rpm;
            else displayed_rpm = displayed_rpm * 0.9 + raw_rpm * 0.1;

            target_uoz = getUOZ(raw_rpm);
            
            unsigned long delay_us = (unsigned long)((float)sector_avg * (120.0 - target_uoz) / 120.0);
            if (delay_us < 50) delay_us = 50;
            if (delay_us > sector_avg - 250) delay_us = sector_avg - 250;
            calculated_delay_us = delay_us;
        }

        if (raw_rpm > 500 && (micros() - signal_timestamp > 3000000)) {
            raw_rpm = 0;
            displayed_rpm = 0;
            calculated_delay_us = 0;
            spark_phase = 0;
            
            sector_sum = 0;
            for (int i = 0; i < 8; i++) {
                sector_buffer[i] = 20000;
                sector_sum += 20000;
            }
            sector_avg = 20000;
            buffer_idx = 0;
            
            timerStop(spark_timer);
            timerWrite(spark_timer, 0);
            GPIO.out_w1ts = (1 << IGNITION_PIN);
        }
    }
}

// ==================== ЯДРО 0: ИСКРА ВМТ ДО 4000 RPM ====================
void VmtSparkTask(void * pvParameters) {
    while (!system_ready) vTaskDelay(10);
    
    float vmt_rpm = 0;
    
    for(;;) {
        if (signal_vmt_core0) {
            signal_vmt_core0 = false;
            
            if (sector_avg > 0) {
                vmt_rpm = 60000000.0 / (float)(sector_avg * 3);
                
                if (vmt_rpm < SPARK_VMT_MAX) {
                    vmtSpark();
                }
            } else {
                vmtSpark();
            }
        }
        
        vTaskDelay(1);
    }
}

// ==================== WEB HANDLERS ====================
void handleRoot() {
    String html = String(INDEX_HTML);
    server.send(200, "text/html", html);
}

void handleGetMaps() {
    String json = "{\"current\":" + String(current_map_index) + ",";
    json += "\"normal\":[";
    for (int i = 0; i < MAP_SIZE; i++) { if (i > 0) json += ","; json += String(uoz_normal[i], 1); }
    json += "],\"sport\":[";
    for (int i = 0; i < MAP_SIZE; i++) { if (i > 0) json += ","; json += String(uoz_sport[i], 1); }
    json += "],\"race\":[";
    for (int i = 0; i < MAP_SIZE; i++) { if (i > 0) json += ","; json += String(uoz_race[i], 1); }
    json += "],\"custom\":[";
    for (int i = 0; i < MAP_SIZE; i++) { if (i > 0) json += ","; json += String(uoz_custom[i], 1); }
    json += "],\"editable\":" + String(current_map_index == 3 ? "true" : "false") + "}";
    server.send(200, "application/json", json);
}

void handleApply() {
    if (server.hasArg("map")) setCurrentMap(server.arg("map").toInt());
    if (current_map_index == 3) {
        for (int i = 0; i < MAP_SIZE; i++) {
            String name = "uoz" + String(i);
            if (server.hasArg(name)) {
                float val = server.arg(name).toFloat();
                if (val >= 0 && val <= 45) uoz_custom[i] = val;
            }
        }
    }
    server.send(200, "text/plain", "OK");
}

void handleSave() {
    prefs.begin("ignition", false);
    for (int i = 0; i < MAP_SIZE; i++) prefs.putFloat(("custom" + String(i)).c_str(), uoz_custom[i]);
    prefs.putInt("current_map", current_map_index);
    prefs.end();
    server.send(200, "text/plain", "Saved");
}

void handleData() {
    const char* mapName = (current_map_index >= 0 && current_map_index < MAP_COUNT) ? map_names[current_map_index] : "Unknown";
    String json = "{\"rpm\":" + String(displayed_rpm, 0) + ",\"uoz\":" + String(target_uoz, 1) + ",\"delay\":" + String(calculated_delay_us) + ",\"map\":" + String(current_map_index) + ",\"mapname\":\"" + String(mapName) + "\"}";
    server.send(200, "application/json", json);
}

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== JetSurf CDI - Dual Timer ===\n");

    btStop();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    pinMode(SENSOR_PIN, INPUT_PULLUP);
    pinMode(IGNITION_PIN, OUTPUT);
    digitalWrite(IGNITION_PIN, HIGH);

    // ===== СОЗДАЁМ ТАЙМЕРЫ ДО ЗАПУСКА ЗАДАЧ! =====
    rpm_timer = timerBegin(RPM_TIMER_IDX, TIMER_DIVIDER, true);
    timerSetAutoReload(rpm_timer, false);
    timerWrite(rpm_timer, 0);

    spark_timer = timerBegin(SPARK_TIMER_IDX, TIMER_DIVIDER, true);
    timerAttachInterrupt(spark_timer, &onSparkTimer, true);
    timerSetAutoReload(spark_timer, false);
    timerWrite(spark_timer, 0);
    timerAlarmEnable(spark_timer);

    WiFi.mode(WIFI_AP);
    WiFi.softAP("JetSurf_CDI", "12345678");

    prefs.begin("ignition", true);
    for (int i = 0; i < MAP_SIZE; i++) {
        uoz_custom[i] = prefs.getFloat(("custom" + String(i)).c_str(), uoz_custom[i]);
    }
    setCurrentMap(prefs.getInt("current_map", 0));
    prefs.end();

    server.on("/", handleRoot);
    server.on("/getmaps", handleGetMaps);
    server.on("/apply", HTTP_POST, handleApply);
    server.on("/save", handleSave);
    server.on("/data", handleData);
    server.begin();

    // Запускаем задачи ПОСЛЕ создания таймеров
    xTaskCreatePinnedToCore(IgnitionLoop, "Ignition", 12288, NULL, configMAX_PRIORITIES - 1, NULL, 1);
    xTaskCreatePinnedToCore(VmtSparkTask, "VmtSpark", 4096, NULL, 1, NULL, 0);

    Serial.println("Ready! IP: 192.168.4.1");
}

void loop() {
    server.handleClient();
    delay(10);
}