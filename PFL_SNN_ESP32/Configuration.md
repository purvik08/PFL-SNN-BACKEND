# Configuration Reference — GeoGuard ESP32-S3

Annotated guide for all system constants, pin mapping, hyperparameters, and FreeRTOS settings defined in `config.h`.

---

## 1. Hardware & Peripheral Pin Mapping (ESP32-S3 N16R8)

```c
// SPI Bus 1: TFT Display (VSPI / SPI2_HOST)
#define TFT_MOSI_PIN 11
#define TFT_SCLK_PIN 12
#define TFT_CS_PIN 10
#define TFT_DC_PIN 13
#define TFT_RST_PIN 14
#define TFT_BLK_PIN 21 // PWM backlight control

// SPI Bus 2: MicroSD Card (HSPI / SPI3_HOST)
#define SD_MOSI_PIN 2
#define SD_MISO_PIN 41
#define SD_SCLK_PIN 42
#define SD_CS_PIN 1

// Status Indicator
#define STATUS_LED_PIN 48 // Built-in RGB LED / GPIO 48
```

> **CRITICAL HARDWARE GUARDRAIL**: GPIO 33, 34, 35, 36, 37 are permanently wired to Octal SPI Flash & PSRAM. Never assign them to any peripheral!

---

## 2. Network & Server Configuration

| Macro | Default Value | Description |
|---|---|---|
| `WIFI_SSID` | `"prajapati 4G"` | Primary Wi-Fi network SSID |
| `WIFI_PASSWORD` | `"medhansh02"` | Wi-Fi WPA2 password |
| `HTTP_PORT` | `80` | Web server & REST API port |
| `FTP_PORT` | `21` | FTP server port for PDF reports |
| `FTP_USER` | `"geoguard"` | FTP authentication username |
| `FTP_PASS` | `"geoguard123"` | FTP authentication password |
| `GEO_API_URL` | `"http://ip-api.com/json/"` | Geolocation REST API endpoint |
| `GEO_TIMEOUT_MS`| `5000` | HTTP client timeout for geolocation |

---

## 3. Embedded SNN Hyperparameters & Tiling

| Macro | Default | Unit / Range | Description |
|---|---|---|---|
| `SNN_PATCH_W` | `128` | pixels | Input patch width (must match model architecture) |
| `SNN_PATCH_H` | `128` | pixels | Input patch height |
| `SNN_NUM_STEPS` | `5` | timesteps | Leaky Integrate-and-Fire (LIF) temporal evaluation steps ($T=5$) |
| `SNN_BETA` | `0.9f` | $[0.0, 1.0]$ | LIF neuron membrane potential decay factor per timestep |
| `SNN_VTH` | `0.5f` | threshold | LIF neuron action potential firing threshold |
| `SNN_CHANGE_THRESH`| `0.5f` | $[0.0, 1.0]$ | Softmax class-1 decision boundary for change identification |
| `SPARSE_DIFF_ENABLE` | `1` | boolean | Enables coarse frame difference pre-check |
| `SPARSE_DIFF_THRESH` | `0.035f` | 3.5% | Skips full SNN if mean absolute difference $|T2 - T1| < 3.5\%$ |

### Channel Architecture
```c
#define ENC_CH0 8        // Layer 0 encoder output channels (128x128)
#define ENC_CH1 16       // Layer 1 encoder output channels (64x64)
#define ENC_CH2 32       // Layer 2 encoder output channels (32x32)
#define ENC_CH3 64       // Layer 3 encoder output channels (16x16)
#define BOTTLENECK_CH 128// Bottleneck feature channels (8x8)
```

---

## 4. Storage & File System

```c
#define SD_MOUNT_POINT "/sdcard"
#define REPORTS_DIR "/reports"
#define MAX_REPORTS 100
```
- Generated PDF reports are saved as `/reports/report_001.pdf`, `/reports/report_002.pdf`, etc.
- Storage medium: MicroSD card over HSPI (FAT32 formatted).

---

## 5. FreeRTOS Task Scheduling & Core Allocation

| Macro | Value | Description |
|---|---|---|
| `TASK_NET_CORE` | `0` | Core 0: Wi-Fi STA, AsyncWebServer, FTP server loop |
| `TASK_INF_CORE` | `1` | Core 1: Vector SNN inference, SD file I/O, TFT rendering |
| `NET_TASK_STACK` | `8192` | Stack size in bytes for Core 0 network task |
| `INF_TASK_STACK` | `16384` | Stack size in bytes for Core 1 SNN inference task |
