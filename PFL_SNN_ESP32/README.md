# GeoGuard — Autonomous Embedded SNN Edge Node

> **Hardware:** ESP32-S3 N16R8 (16 MB Flash · 8 MB Octal PSRAM)
> **Firmware:** PFL_SNN_ESP32
> **Version:** 1.0.0

A fully standalone, edge-deployed **Spiking Neural Network (SNN)** change-detection node. The device ingests two temporal satellite/camera image patches (T1 → T2), runs an INT8-quantized U-Net SNN inference pipeline entirely on-chip, and autonomously generates geo-tagged PDF reports served over Wi-Fi (HTTP) and FTP — with no cloud dependency.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Hardware Requirements](#hardware-requirements)
3. [Software Architecture](#software-architecture)
4. [Module Reference](#module-reference)
5. [Partition Map and Flash Layout](#partition-map-and-flash-layout)
6. [Build and Flash Instructions](#build-and-flash-instructions)
7. [Network Services](#network-services)
8. [Configuration Reference](#configuration-reference)
9. [FreeRTOS Task Architecture](#freertos-task-architecture)
10. [File Structure](#file-structure)

---

## Project Overview

The GeoGuard edge node processes two grayscale satellite/camera patches captured at different times (T1 and T2) through an embedded Siamese U-Net SNN. The network outputs a binary change mask indicating which pixels have changed, then generates a geo-tagged PDF report saved to MicroSD and served via HTTP and FTP.

### Key Capabilities

| Feature | Detail |
|---|---|
| SNN Architecture | INT8-quantized Siamese U-Net with LIF neurons |
| Patch Size | 128 x 128 pixels (single-channel grayscale) |
| LIF Timesteps | T = 5 (Poisson rate coding) |
| Membrane Decay (beta) | 0.9 |
| Firing Threshold (Vth) | 0.5 |
| Sparse Skip | Skips SNN if mean pixel diff < 3.5% |
| Storage | MicroSD card (FAT32), PDF reports up to 100 |
| Web UI | AsyncWebServer on port 80, served from LittleFS |
| FTP Access | Port 21, credentials geoguard / geoguard123 |
| Geolocation | Automatic IP-based via ip-api.com |
| Display | TFT SPI display with live QR code |

---

## Hardware Requirements

| Component | Specification |
|---|---|
| MCU Module | ESP32-S3 N16R8 (16 MB SPI Flash, 8 MB OPI PSRAM) |
| TFT Display | SPI TFT (ST7789 or compatible) |
| MicroSD Module | SPI MicroSD card adapter, FAT32 formatted |
| Status LED | Built-in WS2812 / RGB on GPIO 48 |
| Power | 3.3V regulated (USB or LiPo) |

> **CRITICAL:** GPIO 33, 34, 35, 36, 37 are permanently reserved for Octal SPI Flash and PSRAM. Never assign them to any peripherals.

See [Pinouts.md](Pinouts.md) for the complete wiring diagram.

---

## Software Architecture

`
PFL_SNN_ESP32.ino          <- Arduino entry point, FreeRTOS task launcher
config.h                   <- All pin definitions, hyperparameters, constants
snn_engine.h / .cpp        <- INT8 SNN U-Net inference engine (PSRAM-backed)
model_weights.h            <- Quantized INT8 weights in PROGMEM (~1.2 MB)
pdf_generator.h / .cpp     <- Zero-dependency PDF 1.4 binary stream writer
web_server.h / .cpp        <- AsyncWebServer HTTP API + image ingestion
ftp_server.h / .cpp        <- Non-blocking FTP server for SD card access
tft_display.h / .cpp       <- TFT render functions + QR code generation
geo_locator.h / .cpp       <- IP geolocation via ip-api.com REST API
data/index.html            <- Web UI (served from LittleFS at 0x610000)
`

### Memory Strategy

- **PROGMEM (Flash):** INT8 model weights stored in program flash, never loaded into SRAM.
- **PSRAM (8 MB OPI):** All tensors, weight cache, image buffers (T1/T2), spike accumulators, and change masks are allocated exclusively in PSRAM via heap_caps_malloc(MALLOC_CAP_SPIRAM).
- **Internal SRAM:** Reserved for FreeRTOS, stack frames, and lightweight system state only.

---

## Module Reference

### snn_engine — SNN Inference Engine

The core INT8 SNN U-Net that compares two image patches and outputs a binary change mask.

**Encoder channel progression:** 1 -> 8 -> 16 -> 32 -> 64 -> 128 (bottleneck)
**Decoder channel progression:** 128 -> 64 -> 32 -> 16 -> 8 -> 2 (changed/unchanged)

| Method | Description |
|---|---|
| egin() | Allocates all PSRAM tensors, caches weights from PROGMEM |
| infer(t1, t2, threshold) | Full inference pass; returns SNNResult |
| getChangeMask() | Returns PSRAM pointer to 1-byte-per-pixel binary mask |
| end() | Frees all PSRAM allocations |

### pdf_generator — Embedded PDF Writer

Zero-dependency PDF 1.4 binary stream generator. Writes directly to MicroSD via the SD library. Report contains timestamp, GPS coordinates, change statistics, and inline 128x128 binary mask image.

### web_server — HTTP API

AsyncWebServer on port **80**. Hosts the web UI from LittleFS and provides REST endpoints.

| Endpoint | Method | Description |
|---|---|---|
| / | GET | Serves index.html from LittleFS |
| /ingest | POST | Accepts T1 + T2 patches, triggers SNN inference |
| /report/latest | GET | Downloads the most recent PDF from SD card |
| /status | GET | Returns JSON system status |

### ftp_server — FTP Access

Non-blocking FTP server on port **21**. Reports available under /reports/report_NNN.pdf.
Credentials: **geoguard / geoguard123**

### geo_locator — IP Geolocation

Queries ip-api.com/json/ after Wi-Fi connection. Populates city, country, lat, lon. Timeout: 5000 ms.

### tft_display — TFT Render Interface

| Function | Description |
|---|---|
| 	ftInit() | Initialize SPI bus 1 and display |
| 	ftRenderBoot() | Boot splash screen |
| 	ftRenderNetworkStatus(ip, city) | Show IP and geolocated city |
| 	ftRenderInferenceRunning() | Processing animation |
| 	ftRenderInferenceResult(res) | Change %, pixel count, inference ms |
| 	ftRenderQRCode(url) | Render QR code pointing to download URL |
| 	ftRenderMemoryMetrics() | PSRAM free / heap free display |

---

## Partition Map and Flash Layout

| Address | Size | Name | Description |
|---|---|---|---|
| 0x0 | auto | bootloader | ESP32-S3 bootloader |
| 0x8000 | 4 KB | partitions | Partition table |
| 0xE000 | 8 KB | otadata | OTA boot state |
| 0x10000 | 3 MB | app0 | Main application firmware |
| 0x310000 | 3 MB | app1 | OTA update slot |
| 0x610000 | 6 MB | spiffs | LittleFS web UI files |
| 0xC10000 | 3.9 MB | model | srmodels.bin (ESP-SR) |
| 0xFF0000 | 64 KB | coredump | Crash dump capture |

See [Flash_Layout.md](Flash_Layout.md) for the complete esptool flash command.

---

## Build and Flash Instructions

### Prerequisites

- Arduino IDE 2.x with **ESP32 board package v3.3.8**
- Board: **ESP32S3 Dev Module**
- PSRAM: **OPI PSRAM** (enabled in Tools menu)
- Flash Size: **16 MB**
- Partition Scheme: **Custom** (uses partitions.csv in sketch folder)

### Required Files Before Flashing

| File | Source |
|---|---|
| *.bootloader.bin | Auto-generated by Arduino IDE |
| *.partitions.bin | Auto-generated by Arduino IDE |
| oot_app0.bin | From Arduino15/packages/esp32/hardware/esp32/3.3.8/tools/partitions/ |
| *.ino.bin | Auto-generated by Arduino IDE |
| spiffs.bin | Built with mklittlefs from the data/ folder |
| srmodels.bin | Copied from Arduino15/packages/esp32/tools/esp32s3-libs/3.3.8/esp_sr/ |

See [Flash_Layout.md](Flash_Layout.md) for step-by-step instructions and the ready-to-run flash command.

---

## Network Services

| Service | Port | Notes |
|---|---|---|
| HTTP | 80 | Web UI + REST API |
| FTP | 21 | SD card PDF access |

**Wi-Fi fallback:** If connection fails after 25 retries, starts SoftAP GeoGuard_SNN_Node (password: 12345678) at 192.168.4.1.

---

## Configuration Reference

All constants are in [config.h](config.h). See [Configuration.md](Configuration.md) for the full annotated reference.

---

## FreeRTOS Task Architecture

| | Core 0 (Net Task) | Core 1 (Inference Task) |
|---|---|---|
| Stack | 8 KB | 16 KB |
| Priority | 1 | 2 |
| Responsibilities | Wi-Fi, AsyncWebServer, FTP | SNN inference, SD I/O, TFT rendering |

---

## File Structure

`
PFL_SNN_ESP32/
├── PFL_SNN_ESP32.ino       Main sketch + FreeRTOS entry
├── config.h                Pin map, hyperparameters, constants
├── snn_engine.h / .cpp     SNN U-Net inference engine
├── model_weights.h         INT8 quantized weights (PROGMEM, ~1.2 MB)
├── pdf_generator.h / .cpp  PDF 1.4 binary report writer
├── web_server.h / .cpp     HTTP AsyncWebServer + REST API
├── ftp_server.h / .cpp     Non-blocking FTP server
├── tft_display.h / .cpp    TFT display + QR code renderer
├── geo_locator.h / .cpp    IP geolocation engine
├── data/
│   └── index.html          Web dashboard UI (LittleFS)
├── README.md               This file
├── Pinouts.md              GPIO wiring reference
├── Flash_Layout.md         Partition table + esptool flash commands
└── Configuration.md        config.h annotated reference
`
