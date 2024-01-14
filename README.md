# Hardware Monitor for round 1.28" display

These are available on Aliexpress as "smartwatches". Based on ESP32-C3-MINI and 
a GC9A01 display controller. (By the way, at least my device has quite weak
WiFi)

## Requirements

- ESP-IDF 5.0

### 3rd party components

- LVGL ESP32 drivers (slightly modified): https://github.com/lvgl/lvgl_esp32_drivers (MIT license)
- WiFi Manager is from: https://github.com/huardti/esp32-wifi-manager (MIT license)
- Font from: https://fonts.google.com/specimen/Poppins (Open Font License)
 
## Building and using

First, install [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonito) on your PC
and make sure you either have a static IP or a stable local hostname that resolves correctly.

Simply run `idf.py menuconfig` and set the LibreHardwareMonitor URL (for example the PC
has IP `192.168.1.123`) to:
`http://192.168.1.123:8085/Sensor?action=Get&id=`

Customize the sensors and labels in [sensors.c](main/sensors.c). You can find the sensor paths
by accessing LibreHardwareMonitor's `data.json` (eg. `http://192.168.1.123:8085/data.json`).

Now you can `idf.py build` and `idf.py -p com[n] flash` (keep BOOT button pressed and press
RST to go into boot mode, then RST after flash is complete). The device should initially come
up with an access point (SSID `CircleMonitor` and password `12345678`), connect to that and
connect to your WiFi.

If all goes well, the sensor data should soon start scrolling on the display.

### Clearing NVS

Press BOOT button after releasing RST.

### Changing font

Font was converted from TTF to LVGL format using [`lv_font_conv` ](https://github.com/lvgl/lv_font_conv):

```s
lv_font_conv \
  --font /usr/share/fonts/truetype/Poppins-Medium.ttf \
  -r 0x20-0xFF \
  --size 20 \
  --format lvgl \
  --bpp 1 \
  -o CircleMonitor/main/poppins_medium_20.h
```