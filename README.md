# Smart Industry Monitoring System (ESP32)

A low-cost, ESP32-based IoT node that monitors gas and temperature in an industrial area, shows the status on local displays, reports its location with GPS, and streams live readings to a web dashboard.

## Problem Statement
Industrial areas can develop gas leaks and abnormal temperatures without anyone noticing in time. Large facilities also make it hard to tell *where* an incident is happening. This project provides a low-cost, real-time monitoring and alert node that reports conditions on the spot, identifies the location of an alert, and lets supervisors watch the readings remotely.

## Features
- Live **temperature and humidity** monitoring (DHT22)
- Live **gas level** monitoring (MQ gas sensor)
- Status shown on a **16x2 I2C LCD** and a **0.96" I2C OLED**
- Four status levels: `NORMAL CONDITION` → `DANGER / ALERT` → `DISASTER` → `EVACUATE / ALERT`
- **Sensor-fault detection** (shows `SENSOR FAULT` if the DHT22 stops responding)
- **GPS module** to show the location of the alert across large industrial areas
- **Green / Red LED** indication (red blinks during any alert)
- **Wi-Fi web dashboard** (HTML + CSS + JavaScript) with live readings, status banner, gas history graph, GPS location and alert history

## Hardware Used
| Component | Purpose |
|-----------|---------|
| ESP32 development board | Main microcontroller (with Wi-Fi) |
| DHT22 | Temperature and humidity sensor |
| MQ gas sensor | Gas level detection |
| GY-GPS6MV2 (u-blox NEO-6M) GPS module | Location of the alert |
| 16x2 LCD with I2C backpack (0x27) | Status display |
| 0.96" SSD1306 OLED, I2C (0x3C) | Detailed status display |
| Red and green LEDs | Visual alert indicators |
| Breadboards, jumper wires | Prototyping |

See [`components.md`](components.md) for the full list.

## Pin Connections
| Device | Signal | ESP32 pin |
|--------|--------|-----------|
| DHT22 | Data | GPIO 4 |
| MQ gas sensor | Analog out | GPIO 34 |
| Red LED | Alert | GPIO 26 |
| Green LED | Normal | GPIO 25 |
| GPS module | GPS TX → ESP32 RX | GPIO 16 |
| GPS module | GPS RX ← ESP32 TX | GPIO 17 |
| LCD (I2C) | SDA / SCL | GPIO 21 / GPIO 22 |
| OLED (I2C) | SDA / SCL | GPIO 21 / GPIO 22 |

Power: sensors and displays are powered from the ESP32 3.3V/5V and GND rails as required by each module. Add a resistor in series with each LED. The MQ sensor outputs up to 5V on some modules, so use a voltage divider on its analog output if your module is powered from 5V.

## How It Works
1. The **DHT22** and **MQ sensor** are read every 2 seconds by the ESP32.
2. The readings are compared against the thresholds set in the code.
3. The status is shown on the **LCD** and **OLED**.
4. The **green LED** stays on in normal conditions. The **red LED** blinks when a reading becomes abnormal.
5. The **GPS module** supplies the location. When a fix is available, it is shown with the alert; otherwise the display shows `Location unavailable`.
6. The ESP32 connects to Wi-Fi and serves its readings as JSON at `http://<ESP32-IP>/data`. The dashboard reads this every 2 seconds.

### Alert levels
| Level | Status | Condition (example thresholds) |
|-------|--------|--------------------------------|
| 0 | NORMAL CONDITION | Everything below the limits |
| 1 | DANGER / ALERT | Gas ≥ 1800 **or** temperature ≥ 50 °C |
| 2 | DISASTER | Gas ≥ 2600 **or** temperature ≥ 60 °C |
| 3 | EVACUATE / ALERT | Gas ≥ 2600 **and** temperature ≥ 60 °C |

The thresholds are example values and are **not calibrated gas concentrations**. The gas value is the raw ESP32 ADC reading (0-4095). Calibrate for your sensor and environment before relying on it.

## Software and Libraries
- Arduino IDE (or VS Code with an ESP32 toolchain)
- ESP32 board package by Espressif (includes `WiFi` and `WebServer`)
- Libraries to install:
  - `Wire` (built in)
  - `LiquidCrystal_I2C`
  - `Adafruit GFX Library`
  - `Adafruit SSD1306`
  - `DHT sensor library` (Adafruit)
  - `TinyGPSPlus`

## Setup and Usage
1. Clone this repository:
   ```bash
   git clone https://github.com/devanandpc25/smart_industry_monitor.git
   ```
2. Install the ESP32 board package and the libraries listed above.
3. Wire the components as shown in the pin table.
4. Open `src/smart_industry_monitor/smart_industry_monitor.ino`.
5. Enter your Wi-Fi name and password near the top of the sketch (`WIFI_SSID`, `WIFI_PASS`). Use a 2.4 GHz network.
6. Select your ESP32 board and COM port, then click **Upload**.
7. Open the Serial Monitor (115200 baud). The dashboard address is printed, and the IP also shows on the LCD for 3 seconds at startup.
8. For GPS, place the antenna near a window or outdoors and wait for a fix.

## Dashboard
A single-file dashboard is in [`dashboard monitor.html`](dashboard%20monitor.html). It shows temperature, humidity, gas level, the LED state, a gas history graph, the status banner, the GPS location and an alert history.

- **Demo mode (default):** open the file in any browser. It shows simulated data. Click **Simulate gas leak** to watch the status levels change.
- **Live mode:** connect your computer or phone to the same Wi-Fi as the ESP32, type the address (for example `http://192.168.1.50/data`) and click **Connect**. The ESP32 returns JSON like this:
  ```json
  {"temperature": 28.8, "humidity": 80, "gas": 1650, "level": 0, "fault": false, "lat": null, "lng": null}
  ```
- Edit `GAS_DANGER`, `GAS_DISASTER`, `TEMP_DANGER` and `TEMP_DISASTER` at the top of the dashboard script so they match the thresholds in the sketch.
- Open the file locally for live mode. Browsers block an `https://` page (such as GitHub Pages) from reading a plain `http://` ESP32, so a hosted copy works in demo mode only.

## Project Structure
```
smart_industry_monitor/
├── README.md
├── components.md                         # Components list
├── dashboard monitor.html                # HTML/CSS/JS web dashboard
└── src/
    └── smart_industry_monitor/
        └── smart_industry_monitor.ino    # ESP32 firmware
```

## Limitations
- This is a learning prototype, not a certified safety device. Do not rely on it as the only protection in a real industrial setting.
- The MQ sensor is uncalibrated and cross-sensitive to several gases, and it needs a warm-up period after power-on.
- The dashboard has no login. Use it only on a trusted network.

## Future Improvements
- Cloud logging and remote alerts (SMS / mobile notifications)
- Buzzer for audible alarms
- Sensor calibration for specific gases
- Custom PCB instead of a breadboard
- Mobile app for monitoring

## Author
**Devanand P C**
Electronics and Communication Engineering Student
[LinkedIn](https://www.linkedin.com/in/devanand-p-c-5bb7b5388) | [GitHub](https://github.com/devanandpc25)
