# Flash Layout & Flashing Guide — GeoGuard ESP32-S3

Complete partition map, required binary sources, and step-by-step esptool flashing instructions.

---

## 1. Partition Table

File: `partitions.csv` (located in sketch directory)

```csv
# Name,   Type, SubType,  Offset,    Size,   Flags
nvs,      data, nvs,      0x9000,    0x5000,
otadata,  data, ota,      0xE000,    0x2000,
app0,     app,  ota_0,    0x10000,   0x300000,
app1,     app,  ota_1,    0x310000,  0x300000,
spiffs,   data, spiffs,   0x610000,  0x600000,
model,    data, spiffs,   0xC10000,  0x3E0000,
coredump, data, coredump, 0xFF0000,  0x10000,
```

### Memory Map Layout (16 MB Total)

```
0x000000 +----------------------------------------------+
         | Bootloader (~32 KB)                          |
0x008000 +----------------------------------------------+
         | Partition Table (4 KB)                       |
0x009000 +----------------------------------------------+
         | NVS (20 KB) -- Wi-Fi credentials, state      |
0x00E000 +----------------------------------------------+
         | otadata (8 KB) -- OTA boot descriptor        |
0x010000 +----------------------------------------------+
         | app0 (3 MB) -- GeoGuard SNN Firmware (Active)|
0x310000 +----------------------------------------------+
         | app1 (3 MB) -- OTA Secondary Slot            |
0x610000 +----------------------------------------------+
         | spiffs (6 MB) -- LittleFS (Web UI & Assets)  |
0xC10000 +----------------------------------------------+
         | model (3.9 MB) -- ESP-SR Speech/SNN Model    |
0xFF0000 +----------------------------------------------+
         | coredump (64 KB) -- System Crash Log Dump    |
0x1000000+----------------------------------------------+
```

---

## 2. Required Binaries Before Flashing

| File | Flash Offset | Origin / Generation Command |
|---|---|---|
| `PFL_SNN_ESP32.ino.bootloader.bin` | `0x0` | Generated during Arduino IDE compilation |
| `PFL_SNN_ESP32.ino.partitions.bin` | `0x8000` | Generated from `partitions.csv` |
| `boot_app0.bin` | `0xE000` | Found in ESP32 package (`tools/partitions/boot_app0.bin`) |
| `PFL_SNN_ESP32.ino.bin` | `0x10000` | Compiled application binary |
| `spiffs.bin` | `0x610000` | Generated using `mklittlefs` from `data/` folder |
| `srmodels.bin` | `0xC10000` | Copied from ESP-SR package (`esp32s3-libs/3.3.8/esp_sr/`) |

---

## 3. Preparation Steps

### Step A: Generate `spiffs.bin` (LittleFS Image)
```cmd
"C:\Users\MacBook\AppData\Local\Arduino15\packages\esp32\tools\mklittlefs\4.0.2-db0513a\mklittlefs.exe" -c "e:\GeoGuard\PFL-SNN-BACKEND\PFL_SNN_ESP32\data" -s 0x600000 -b 4096 -p 256 "C:\Users\MacBook\AppData\Local\arduino\sketches\4B8E452264A2637A2C8F90515C4289FA\spiffs.bin"
```

### Step B: Copy `srmodels.bin` to Build Folder
```cmd
copy "C:\Users\MacBook\AppData\Local\Arduino15\packages\esp32\tools\esp32s3-libs\3.3.8\esp_sr\srmodels.bin" "C:\Users\MacBook\AppData\Local\arduino\sketches\4B8E452264A2637A2C8F90515C4289FA\srmodels.bin"
```

---

## 4. Complete Flashing Command (esptool)

> **IMPORTANT**: Ensure the serial monitor and Arduino IDE upload terminal are closed before running to avoid COM port access conflicts.

```cmd
cmd.exe /c ""C:\Users\MacBook\AppData\Local\Arduino15\packages\esp32\tools\esptool_py\5.2.0\esptool.exe" --chip esp32s3 --port COM4 --baud 921600 --before default-reset --after hard-reset write-flash -e -z --flash-mode keep --flash-freq keep --flash-size keep 0x0 "C:\Users\MacBook\AppData\Local\arduino\sketches\4B8E452264A2637A2C8F90515C4289FA\PFL_SNN_ESP32.ino.bootloader.bin" 0x8000 "C:\Users\MacBook\AppData\Local\arduino\sketches\4B8E452264A2637A2C8F90515C4289FA\PFL_SNN_ESP32.ino.partitions.bin" 0xe000 "C:\Users\MacBook\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.8\tools\partitions\boot_app0.bin" 0x10000 "C:\Users\MacBook\AppData\Local\arduino\sketches\4B8E452264A2637A2C8F90515C4289FA\PFL_SNN_ESP32.ino.bin" 0x610000 "C:\Users\MacBook\AppData\Local\arduino\sketches\4B8E452264A2637A2C8F90515C4289FA\spiffs.bin" 0xC10000 "C:\Users\MacBook\AppData\Local\arduino\sketches\4B8E452264A2637A2C8F90515C4289FA\srmodels.bin""
```

---

## 5. Fast App-Only Re-Flash (Iterative Development)

When only C++ source code has changed:

```cmd
cmd.exe /c ""C:\Users\MacBook\AppData\Local\Arduino15\packages\esp32\tools\esptool_py\5.2.0\esptool.exe" --chip esp32s3 --port COM4 --baud 921600 --before default-reset --after hard-reset write-flash -z --flash-mode keep --flash-freq keep --flash-size keep 0x10000 "C:\Users\MacBook\AppData\Local\arduino\sketches\4B8E452264A2637A2C8F90515C4289FA\PFL_SNN_ESP32.ino.bin""
```

---

## 6. Common Diagnostics & Troubleshooting

| Symptom | Cause | Solution |
|---|---|---|
| `Access is denied (COM4)` | Another program has open COM handle | Close Arduino IDE Serial Monitor / Putty / VSCode |
| `No such file or directory: srmodels.bin` | File not copied to active sketch cache | Re-run Step B to copy `srmodels.bin` |
| `LittleFS Mount Failed` | `spiffs.bin` not flashed or corrupted | Flash `spiffs.bin` to `0x610000` with erase `-e` |
| `PSRAM not found! Ensure OPI PSRAM is enabled` | Wrong board settings in IDE | In Arduino IDE: Tools -> PSRAM -> OPI PSRAM |
