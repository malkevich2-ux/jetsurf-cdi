#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SDA_PIN 12
#define SCL_PIN 13

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define GPS_RX 5
#define BUTTON_PIN 10   // Сброс максимальной скорости
#define RESET_PIN 11    // Программный перезапуск ESP32

float gps_speed = 0;
int gps_sats = 0;
bool gps_fix = false;
String gps_line = "";
String gps_time = "";

float max_speed = 0;
bool show_max = false;
unsigned long last_button = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(RESET_PIN, INPUT_PULLUP);

    Wire.begin(SDA_PIN, SCL_PIN);
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        while (1);
    }
    
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(15, 20);
    display.println("JetSurf");
    display.setTextSize(1);
    display.setCursor(20, 45);
    display.println("GPS Starting...");
    display.display();

    Serial1.begin(9600, SERIAL_8N1, GPS_RX, -1);
    delay(2000);
}

void loop() {
    // Кнопка RESET
    if (digitalRead(RESET_PIN) == LOW) {
        delay(50);
        if (digitalRead(RESET_PIN) == LOW) {
            display.clearDisplay();
            display.setTextSize(2);
            display.setCursor(20, 25);
            display.println("RESET...");
            display.display();
            delay(500);
            ESP.restart();
        }
    }

    // Кнопка сброса макс. скорости
    if (digitalRead(BUTTON_PIN) == LOW) {
        if (millis() - last_button > 300) {
            last_button = millis();
            max_speed = 0;
            show_max = true;
            delay(1000);
            show_max = false;
        }
    }

    // GPS - только вывод в Serial Monitor
    while (Serial1.available()) {
        char c = Serial1.read();
        Serial.write(c);  // ← ВСЁ с GPS в монитор порта
        
        if (c == '\n') {
            gps_line.trim();
            
            if (gps_line.startsWith("$GPGGA") || gps_line.startsWith("$GNGGA")) {
                int commas[15] = {0}, cnt = 0;
                for (int i = 0; i < gps_line.length() && cnt < 15; i++) {
                    if (gps_line[i] == ',') commas[cnt++] = i;
                }
                if (cnt >= 7) {
                    if (cnt >= 1) {
                        gps_time = gps_line.substring(commas[0]+1, commas[1]);
                        if (gps_time.length() >= 6) {
                            gps_time = gps_time.substring(0,2) + ":" + gps_time.substring(2,4) + ":" + gps_time.substring(4,6);
                        }
                    }
                    String satStr = gps_line.substring(commas[6]+1, commas[7]);
                    if (satStr.length() > 0) gps_sats = satStr.toInt();
                }
            }
            
            if (gps_line.startsWith("$GPRMC") || gps_line.startsWith("$GNRMC")) {
                int idx = 0;
                String fields[13];
                for (int i = 0; i < 13; i++) {
                    int n = gps_line.indexOf(',', idx);
                    if (n == -1) break;
                    fields[i] = gps_line.substring(idx, n);
                    idx = n + 1;
                }
                
                gps_fix = (fields[2] == "A");
                
                if (fields[7].length() > 0) {
                    gps_speed = fields[7].toFloat() * 1.852;
                } else {
                    gps_speed = 0;
                }
            }
            gps_line = "";
        } else if (c != '\r') {
            gps_line += c;
        }
    }

    if (gps_speed > max_speed) max_speed = gps_speed;

    // OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("S:"); display.print(gps_sats);
    display.print(" "); display.print(gps_fix ? "FIX" : "NO");
    display.setCursor(70, 0);
    display.print(gps_time);
    display.drawLine(0, 10, 128, 10, WHITE);
    
    if (show_max) {
        display.setTextSize(2);
        display.setCursor(10, 25); display.print("MAX");
        display.setTextSize(3);
        display.setCursor(15, 35); display.print(max_speed, 0);
        display.setTextSize(1);
        display.setCursor(100, 50); display.print("km/h");
    } else {
        display.setTextSize(4);
        if (gps_speed < 10) display.setCursor(20, 20);
        else if (gps_speed < 100) display.setCursor(5, 20);
        else display.setCursor(0, 20);
        display.print(gps_speed, 1);
        display.setTextSize(2);
        display.setCursor(80, 30); display.print("km/h");
        display.setTextSize(1);
        display.setCursor(0, 56);
        display.print("MAX:"); display.print(max_speed, 0); display.print(" km/h");
    }
    display.display();
    delay(50);
}