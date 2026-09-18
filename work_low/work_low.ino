#include <WiFi.h>
#include <WebServer.h> 

#include <Preferences.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <driver/gpio.h>
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

const int SPARK_PULSE_US = 100;

// ==================== СТРУКТУРЫ ====================
struct MapPoint {
  int rpm;
  float uoz;
};
MapPoint uoz_map[MAP_SIZE];

static portMUX_TYPE ignitionMux = portMUX_INITIALIZER_UNLOCKED;

// ==================== ПЕРЕМЕННЫЕ ЗАЖИГАНИЯ ====================
volatile unsigned long full_cycle_buffer[16] = {0};
volatile byte full_buffer_idx = 0;
volatile unsigned long full_cycle_sum = 0;
volatile unsigned long full_cycle_avg = 100000;  // Начальное значение (600 RPM)

volatile unsigned long last_240_time = 0;
volatile unsigned long last_vmt_time = 0;

volatile bool has_valid_signal = false;
volatile unsigned long last_signal_time = 0;

volatile unsigned long calculated_delay = 0;
volatile float calculated_uoz = 12.0;
volatile bool need_calculation = false;
volatile bool normal_mode_locked = false;


volatile float current_rpm = 0;
volatile int max_rpm_limit = 15000; 
volatile int soft_rpm_limit = 20600;
volatile bool hard_cut_active = false;
volatile bool direct_spark_at_vmt = true;

// Wi-Fi управление
volatile bool wifi_enabled = true;
volatile int wifi_off_rpm = 3000;
unsigned long wifi_debounce_timer = 0;

// ==================== РЕЖИМЫ ====================
enum EngineMode { MODE_START, MODE_NORMAL, MODE_AIR, MODE_WATER, MODE_SOFT_CUT, MODE_HARD_CUT, MODE_DRY };
const char* mode_names[] = { "START", "NORMAL", "AIR", "WATER", "SOFT_LIMIT", "HARD_LIMIT", "DRY" };
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

esp_timer_handle_t delay_timer_handle = NULL;
esp_timer_handle_t pulse_timer_handle = NULL;

float last_cached_rpm = -1.0;
float last_cached_uoz = 0.0;

volatile bool system_ready = false;

// ==================== ФУНКЦИЯ ДЛЯ ПОДАВЛЕНИЯ ШУМОВ ====================
void setupPinsForNoiseReduction() {
    pinMode(SENSOR_PIN, INPUT_PULLUP);
    pinMode(IGNITION_PIN, OUTPUT);
    digitalWrite(IGNITION_PIN, HIGH);
    
    }

// ==================== ФУНКЦИИ ====================

void IRAM_ATTR onDelayTimer(void* arg) {
  digitalWrite(IGNITION_PIN, LOW);
  esp_timer_start_once(pulse_timer_handle, SPARK_PULSE_US); 
}

void IRAM_ATTR onPulseTimer(void* arg) {
  digitalWrite(IGNITION_PIN, HIGH);
}

float getInterpolatedUOZ(float rpm) {
  if (fabs(rpm - last_cached_rpm) < 5.0) {
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
  
  if (result < 0) result = 0;
  if (result > 35) result = 35;
  
  last_cached_rpm = rpm;
  last_cached_uoz = result;
  return result;
}

// ==================== ISR ====================
void IRAM_ATTR sensorISR(void* arg) {
    unsigned long now = micros();
    static unsigned long last_interrupt = 0;
    
    // Увеличенный дебаунс для защиты от помех
    if (now - last_interrupt < 50) return;
    last_interrupt = now;

    bool pin_state = gpio_get_level((gpio_num_t)SENSOR_PIN);
    
    if (pin_state == HIGH) {
        // === 240° ===
        last_240_time = now;
        esp_timer_stop(delay_timer_handle);
        
        if (!direct_spark_at_vmt && calculated_delay > 0 && calculated_delay < 500000 && system_ready) {
            esp_timer_start_once(delay_timer_handle, calculated_delay);
        }
        
    } else if (pin_state == LOW) {
        // === ВМТ ===
        
        // 1. Измеряем ПОЛНЫЙ цикл (от предыдущего ВМТ до текущего)
        if (last_vmt_time > 0) {
            unsigned long full_cycle = now - last_vmt_time;
            
            // Фильтр: от 5 мс до 2 секунд (0.5 RPM до 20000 RPM)
            if (full_cycle > 1000 && full_cycle < 2000000) {
                portENTER_CRITICAL_ISR(&ignitionMux);
                unsigned long oldest = full_cycle_buffer[full_buffer_idx];
                full_cycle_sum = full_cycle_sum - oldest + full_cycle;
                full_cycle_buffer[full_buffer_idx] = full_cycle;
                full_buffer_idx = (full_buffer_idx + 1) & 15;
                full_cycle_avg = full_cycle_sum >> 4;
                portEXIT_CRITICAL_ISR(&ignitionMux);
                
                has_valid_signal = true;
                last_signal_time = millis();
                need_calculation = true;
            }
        }
        last_vmt_time = now;
        
        // 2. Прямой удар искры для низких оборотов
        if (direct_spark_at_vmt && system_ready) {
            esp_timer_stop(delay_timer_handle);
            digitalWrite(IGNITION_PIN, LOW);
            esp_timer_start_once(pulse_timer_handle, SPARK_PULSE_US);
        }
    }
}

// ==================== ЗАДАЧА ЗАЖИГАНИЯ ====================
void IgnitionLoop(void * pvParameters) {
  gpio_config_t io_conf = {};
  io_conf.intr_type = GPIO_INTR_ANYEDGE;
  io_conf.pin_bit_mask = (1ULL << SENSOR_PIN);
  io_conf.mode = GPIO_MODE_INPUT;
  io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&io_conf);

  pinMode(IGNITION_PIN, OUTPUT);
  digitalWrite(IGNITION_PIN, HIGH);
  
  const esp_timer_create_args_t delay_args = { .callback = &onDelayTimer, .name = "spark_delay" };
  const esp_timer_create_args_t pulse_args = { .callback = &onPulseTimer, .name = "spark_pulse" };
  
  if (esp_timer_create(&delay_args, &delay_timer_handle) != ESP_OK) {
    Serial.println("FATAL: Failed to create delay timer!");
    esp_restart();
  }
  if (esp_timer_create(&pulse_args, &pulse_timer_handle) != ESP_OK) {
    Serial.println("FATAL: Failed to create pulse timer!");
    esp_restart();
  }
  
  esp_task_wdt_add(NULL); 
  
  gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
  gpio_isr_handler_add((gpio_num_t)SENSOR_PIN, sensorISR, NULL);
  
  system_ready = true;
  
  unsigned long last_dry_spark = 0;
  const unsigned long DRY_INTERVAL = 25000;
  
  for(;;) {
    esp_task_wdt_reset(); 
    
    // Режим просушки
    if (dry_mode) {
      if (millis() - dry_start_time > DRY_DURATION) {
        dry_mode = false;
        current_mode = MODE_START;
        direct_spark_at_vmt = true;
      } else {
        current_mode = MODE_DRY;
        current_rpm = DRY_RPM;
        direct_spark_at_vmt = false;
        
        unsigned long now_us = micros();
        if (now_us - last_dry_spark >= DRY_INTERVAL) {
          last_dry_spark = now_us;
          float target_uoz = getInterpolatedUOZ(DRY_RPM);
          unsigned long delay_us = (unsigned long)((float)DRY_INTERVAL * (120.0 - target_uoz) / 360.0);
          if (delay_us < 10) delay_us = 10;
          if (delay_us > DRY_INTERVAL - 100) delay_us = DRY_INTERVAL - 100;
          if (!hard_cut_active && system_ready) {
            esp_timer_start_once(delay_timer_handle, delay_us);
          }
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);
        continue;
      }
    }
    
    // Таймаут сигнала
    if (has_valid_signal && (millis() - last_signal_time > 3000)) {
      has_valid_signal = false;
      current_rpm = 0;
      calculated_delay = 0;
      normal_mode_locked =false;
      current_mode = MODE_START;
      direct_spark_at_vmt = true;
      full_cycle_avg = 100000;
      full_cycle_sum = 0;
      for (int i = 0; i < 16; i++) {
        full_cycle_buffer[i] = 100000;
        full_cycle_sum += 100000;
      }
    }
    
    // ===== РАСЧЕТ ОБОРОТОВ И УОЗ =====
    if (need_calculation) {
      need_calculation = false;
      
      portENTER_CRITICAL(&ignitionMux);
      unsigned long avg_cycle = full_cycle_avg;
      portEXIT_CRITICAL(&ignitionMux);
      
      if (!has_valid_signal || avg_cycle == 0 || avg_cycle < 5000 || avg_cycle > 2000000) {
        current_rpm = 0;
        current_mode = MODE_START;
        calculated_delay = 0;
        direct_spark_at_vmt = true;
        continue;
      }
      
      // ===== РАСЧЕТ RPM =====
      float rpm = 60000000.0 / (float)avg_cycle;
      
      // Отсекаем явно ложные значения (> 20000 RPM)
      if (rpm > 20000) {
        current_rpm = 0;
        current_mode = MODE_START;
        calculated_delay = 0;
        direct_spark_at_vmt = true;
        continue;
      }
      
      current_rpm = rpm;
      
      // Получаем короткий сектор для расчета задержки
      unsigned long short_sector = avg_cycle / 3;  // 120° = 360°/3
      
      // ===== НАЧАЛО: РЕЖИМ НИЗКИХ ОБОРОТОВ (отсечки ОТКЛЮЧЕНЫ) =====
      bool is_starting = (rpm < 700);
      float target_uoz = 0;
      bool spark_allowed = true;
      
      if (is_starting && !normal_mode_locked) {
    current_mode = MODE_START;
    hard_cut_active = false;
    target_uoz = 12.0;  // Фиксированный УОЗ для запуска
    
    direct_spark_at_vmt = true;
    calculated_delay = 0;
    calculated_uoz = target_uoz;
    continue;
}
      
      // ===== ПОСЛЕ СТАРТА: РАБОЧИЙ РЕЖИМ =====
      direct_spark_at_vmt = false;
      
      // Жесткая отсечка
      //if (rpm >= max_rpm_limit) {
       // hard_cut_active = true;
       // current_mode = MODE_HARD_CUT;
       // spark_allowed = false;
       // target_uoz = 0;
      //} else {
        //hard_cut_active = false;
      //}
      
      // Мягкая отсечка
     // if (!hard_cut_active && rpm >= soft_rpm_limit) {
     //   current_mode = MODE_SOFT_CUT;
     //   float overshoot = rpm - soft_rpm_limit;
     //   float window = max_rpm_limit - soft_rpm_limit;
     //   if (window < 50) window = 50;
     //   float reduction = overshoot * 30.0 / window;
     //   if (reduction > 30) reduction = 30;
     //   target_uoz = getInterpolatedUOZ(rpm) - reduction;
     //   if (target_uoz < 0) target_uoz = 0;
      //}
      current_mode = MODE_NORMAL;
      target_uoz = getInterpolatedUOZ(rpm);
      normal_mode_locked = true;
      
      if (spark_allowed) {
        calculated_uoz = target_uoz;
        unsigned long delay_us = (unsigned long)((float)short_sector * (120.0 - target_uoz) / 120.0);
        if (delay_us < 10) delay_us = 10;
        if (delay_us > short_sector - 20) delay_us = short_sector - 20;
        calculated_delay = delay_us;
      } else {
        calculated_delay = 0;
      }
    }
    
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

// ==================== WEB HANDLERS ====================

void handleRoot() {
  String html = String(INDEX_HTML);
  String maxRpmStr = String(max_rpm_limit);
  String softRpmStr = String(soft_rpm_limit);
  html.replace("%MAX_RPM%", maxRpmStr);
  html.replace("%SOFT_RPM%", softRpmStr);
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
    if (val >= 5000 && val <= 16000) max_rpm_limit = val;
  }
  if(server.hasArg("soft_rpm")) {
    int val = server.arg("soft_rpm").toInt();
    if (val >= 4000 && val <= 20600) soft_rpm_limit = val;
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
  for (int i = 0; i < MAP_SIZE; i++) {
    prefs.putFloat(("uoz" + String(i)).c_str(), uoz_map[i].uoz);
  }
  prefs.end();
  server.send(200, "text/plain", "OK");
}

void handleData() {
  String json = "{\"rpm\":" + String(current_rpm, 0) + 
                ",\"mode\":\"" + String(mode_names[current_mode]) + "\"" +
                ",\"wifi_off_rpm\":" + String(wifi_off_rpm) +
                ",\"wifi_enabled\":" + String(wifi_enabled ? 1 : 0) +
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
    server.send(200, "text/plain", "Dry mode started");
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
  String json = "{\"hard\":" + String(max_rpm_limit) + ",\"soft\":" + String(soft_rpm_limit) + "}";
  server.send(200, "application/json", json);
}





// ==================== SETUP ====================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Starting JetSurf CDI...");
  
  // Инициализация буфера для защиты от ложных значений
  for (int i = 0; i < 16; i++) {
    full_cycle_buffer[i] = 100000;  // ~600 RPM
    full_cycle_sum += 100000;
  }
  full_cycle_avg = 100000;
  
  btStop();
  delay(100);
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    esp_bt_controller_disable();
    delay(100);
  }
  esp_bt_controller_deinit();
  
  setupPinsForNoiseReduction();
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP("JetSurf_Smart_CDI", "12345678");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  
 float default_uoz[MAP_SIZE] = {8, 12, 16, 20, 24, 27, 29, 31, 32, 32, 31, 29, 26, 22, 18};

 for(int i = 0; i < MAP_SIZE; i++) {
     uoz_map[i].rpm = RPM_START + (i * RPM_STEP);
     uoz_map[i].uoz = default_uoz[i];
  }
  
  prefs.begin("ignition", false);
  max_rpm_limit = prefs.getInt("max_rpm", 15000);
  soft_rpm_limit = prefs.getInt("soft_rpm", 20600);
  wifi_off_rpm = prefs.getInt("wifi_off_rpm", 3000);
  if (prefs.isKey("map_saved")) {
    for (int i = 0; i < MAP_SIZE; i++) {
      uoz_map[i].uoz = prefs.getFloat(("uoz" + String(i)).c_str(), uoz_map[i].uoz);
    }
  }
  prefs.end();
  
  server.on("/", handleRoot);
  server.on("/apply", HTTP_POST, handleApply);
  server.on("/save", handleSave);
  server.on("/data", handleData);
  server.on("/map", handleMap);
  
  server.on("/dry", HTTP_GET, handleDryMode);
  server.on("/dry/status", HTTP_GET, handleDryStatus);
  
  server.on("/wifi", HTTP_POST, handleWifiSettings);
  server.begin();
  
 
  
  esp_task_wdt_init(10, true);
  
  xTaskCreatePinnedToCore(IgnitionLoop, "IgnitionTask", 16384, NULL, configMAX_PRIORITIES - 1, &IgnitionTaskHandle, 1);
  
  Serial.println("Ready! Connect to JetSurf_Smart_CDI");
  Serial.println("Open http://192.168.4.1");
}

// ==================== LOOP ====================

void loop() {
  
  server.handleClient();
  
  
  // Отладка
  static unsigned long last_debug = 0;
  if (millis() - last_debug > 500) {
    last_debug = millis();
    Serial.print("RPM: ");
    Serial.print(current_rpm, 0);
    Serial.print(" | Mode: ");
    Serial.print(mode_names[current_mode]);
    Serial.print(" | avg_cycle: ");
    Serial.print(full_cycle_avg);
    Serial.print(" | has_signal: ");
    Serial.println(has_valid_signal ? "YES" : "NO");
  }
  
  vTaskDelay(10 / portTICK_PERIOD_MS);
}