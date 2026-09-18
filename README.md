# JetSurf ESP32 CDI Suite

A collection of ESP32-based firmware projects for a JetSurf-style powered surfboard: several
Wi-Fi-tunable **CDI (Capacitor Discharge Ignition)** controllers and a standalone **GPS speedometer**
dashboard. Each ignition controller reads a crank/flywheel position sensor, computes engine RPM,
looks up the target ignition advance angle from a tunable map, and fires the spark coil at the
right moment - all while serving a mobile-friendly web UI over its own Wi-Fi access point so the
map can be tuned from a phone, without re-flashing.

## How the ignition logic works (common to all CDI variants)

- A single sensor (`SENSOR_PIN`, GPIO 17) produces two edges per engine revolution: one at a fixed
  crank angle before top dead center (referred to as "240 degrees" in the code/comments) and one at
  top dead center (TDC).
- The time between these edges is used to measure engine speed (RPM), smoothed with a small
  rolling-average ring buffer to reject noise.
- RPM is fed into an ignition-advance table (called "UOZ" in the code, from the Russian
  "угол опережения зажигания" - ignition advance angle) with linear interpolation between
  calibration points (usually every 1000 RPM).
- The advance angle is converted into a delay (in microseconds) from the "240 degree" edge, and a
  hardware or software timer fires the spark on `IGNITION_PIN` (GPIO 19) after that delay.
- At very low RPM (cranking/idle) some variants fire the spark directly at TDC instead of
  calculating a delay, since the timing math is unreliable at very low, noisy signal frequencies.
- Each controller boots as a Wi-Fi access point (default password `12345678`) and serves a web page
  for live monitoring (RPM, mode, computed advance) and for editing/saving the ignition map to
  flash (NVS, via the `Preferences` library) so tuning survives a power cycle.

## Projects

| Folder | Description |
| --- | --- |
| [`igninion_low/`](igninion_low) | Minimal CDI firmware with two built-in maps (Standard / Sport), switchable from the web page. The smallest and simplest of the set - a good starting point for reading the codebase. |
| [`igninion_graff/`](igninion_graff) | "JetSurf Smart CDI v2.2" - a full-featured build named after its graph-based tuning UI ("graff" = graph): a 15-point editable ignition-advance curve rendered as a chart, sensor mounting-offset correction (+/-3 degrees), soft/hard RPM limiter, a spark-plug "dry mode", automatic Wi-Fi shutdown above a set RPM (to reduce interference while riding), and OTA (over-the-air) firmware updates. |
| [`igninion_graff_all_timer/`](igninion_graff_all_timer) | Same "Smart CDI v2.2" firmware as `igninion_graff/`, kept as a separate checkpoint/backup from development. The source in this folder is currently identical to `igninion_graff/`. |
| [`atom_timer_probs/`](atom_timer_probs) | Experimental **dual hardware-timer** architecture running across both ESP32 cores: one core measures RPM and schedules the advance timer, the other independently fires a direct low-RPM spark. Includes four selectable maps (Normal / Sport / Race / a fully custom user-editable map). Used to prototype timing changes before they were folded into the other variants. |
| [`work_low/`](work_low) | A working tuning build (serves the same "JetSurf Pro CDI" web UI) based on software (`esp_timer`) timers rather than raw hardware timers, with a start/run mode latch that avoids re-triggering start-up behavior once the engine is already running. RPM limiter logic is present in the code but currently commented out in this build. |
| [`pult/`](pult) | A separate device: a handlebar/dash **GPS speedometer**. Reads NMEA sentences (`$GPGGA`/`$GPRMC`) from a GPS module, drives an SSD1306 OLED display with current speed (km/h), satellite count and fix status, and tracks a resettable top speed for a run. Includes a hardware reset button for the ESP32. |

## Hardware notes

- MCU: ESP32 (Arduino core), using `WiFi.h`, `WebServer.h`, `Preferences.h`, and in most variants
  `driver/gpio.h` / `driver/timer.h` or `esp_timer.h` for precise, interrupt-driven spark timing.
- `SENSOR_PIN` = GPIO 17 (crank/flywheel position input, pulled up).
- `IGNITION_PIN` = GPIO 19 (drives the ignition coil/CDI trigger, active low in most variants).
- Bluetooth is explicitly disabled at boot in the more advanced variants to free up CPU time and
  reduce electrical noise near the ignition circuitry.
- The `igninion_graff*` and `work_low` variants set every otherwise-unused GPIO to `INPUT_PULLDOWN`
  at boot as a noise-reduction measure in an electrically noisy two-stroke engine bay.
- `pult/` uses a separate ESP32 wired to an SSD1306 OLED (I2C, SDA=12/SCL=13) and a GPS module on a
  UART (`Serial1`, RX=5).

## Web interface

Connect to the board's Wi-Fi access point (SSID/password printed in each sketch, default
`12345678`) and open `http://192.168.4.1/`. Depending on the variant, the page offers:

- Live telemetry: RPM, current mode (start / idle / normal / soft limit / hard limit / dry),
  computed advance angle, spark count.
- An editable ignition-advance chart (enter points per RPM bracket), with an "Apply" action
  (push to RAM for live testing) and a "Save" action (persist to flash).
- Soft/hard RPM limiter thresholds.
- Sensor mounting-offset trim (`igninion_graff*`).
- Spark-plug "dry mode" - a timed low-frequency spark sequence to help dry a flooded plug.
- Automatic Wi-Fi shutdown above a configurable RPM, to avoid radio interference at speed, with
  automatic re-enable once the engine stops (`igninion_graff*`, `work_low`).
- OTA firmware upload (`/ota`, `igninion_graff*`).

## Safety notice

This is DIY ignition-timing firmware for a small engine. Incorrect ignition advance, a "hard cut"
limiter that's been disabled by mistake, or a firmware bug can damage an engine or cause unsafe
operation of a personal watercraft. Bench-test any change with the engine off or at low, controlled
RPM before relying on it on the water, keep a mechanical/manual kill switch available, and treat
the code here as a personal project, not a certified or production-ready product.

## Repository history

These sketches were pulled together from an existing local project folder that also contained
compiled build artifacts (`.bin`/`.elf`/`.map`) and several nested work-in-progress copies of the
`igninion_graff*` sketches (`build/`, `proverka/`, `expiriment/`, timestamped copies, etc.). Only
the source files (`.ino` + `web_page.h`) needed to build each firmware are kept here; build
artifacts and superseded drafts were intentionally left out to keep the history readable.
