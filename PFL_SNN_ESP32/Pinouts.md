# Pinouts — GeoGuard ESP32-S3 N16R8 Edge Node

All GPIO assignments for the GeoGuard embedded SNN node. Defined in [config.h](config.h).

---

## CRITICAL Reserved Pins

> **DO NOT use GPIO 33, 34, 35, 36, 37 for ANY peripheral.**
> These are permanently wired to the Octal SPI Flash and PSRAM bus on N16R8 modules.

---

## SPI Bus 1 — TFT Display (VSPI / SPI2_HOST)

| Signal | GPIO | Notes |
|---|---|---|
| MOSI (Data Out) | **GPIO 11** | TFT data input |
| SCLK (Clock) | **GPIO 12** | SPI clock |
| CS (Chip Select) | **GPIO 10** | Active LOW |
| DC (Data/Command) | **GPIO 13** | HIGH = data, LOW = command |
| RST (Reset) | **GPIO 14** | Active LOW reset |
| BLK (Backlight) | **GPIO 21** | PWM brightness control |

### Wiring Diagram — TFT Display

`
ESP32-S3          TFT Module
─────────         ──────────────
GPIO 11  ──────►  MOSI / SDA
GPIO 12  ──────►  SCLK / SCL
GPIO 10  ──────►  CS
GPIO 13  ──────►  DC / RS
GPIO 14  ──────►  RST / RES
GPIO 21  ──────►  BLK / LED
3.3V     ──────►  VCC
GND      ──────►  GND
`

**Compatible drivers:** ST7789, ILI9341, ST7735 (any SPI TFT)

---

## SPI Bus 2 — MicroSD Card (HSPI / SPI3_HOST)

| Signal | GPIO | Notes |
|---|---|---|
| MOSI (Data Out) | **GPIO 2** | SD card data input |
| MISO (Data In) | **GPIO 41** | SD card data output |
| SCLK (Clock) | **GPIO 42** | SPI clock |
| CS (Chip Select) | **GPIO 1** | Active LOW |

### Wiring Diagram — MicroSD

`
ESP32-S3          MicroSD Adapter
─────────         ───────────────
GPIO 2   ──────►  MOSI / CMD
GPIO 41  ◄──────  MISO / DAT0
GPIO 42  ──────►  SCLK / CLK
GPIO 1   ──────►  CS
3.3V     ──────►  VCC
GND      ──────►  GND
`

> **Note:** MicroSD card must be formatted as **FAT32**. SPI clock set to 25 MHz.

---

## Status Indicator

| Signal | GPIO | Notes |
|---|---|---|
| RGB LED / Status | **GPIO 48** | Built-in addressable LED (WS2812-compatible) |

---

## Reserved / Forbidden Pins

| GPIO | Reason | Risk |
|---|---|---|
| GPIO 33 | Octal PSRAM / Flash bus | Permanent hardware conflict |
| GPIO 34 | Octal PSRAM / Flash bus | Permanent hardware conflict |
| GPIO 35 | Octal PSRAM / Flash bus | Permanent hardware conflict |
| GPIO 36 | Octal PSRAM / Flash bus | Permanent hardware conflict |
| GPIO 37 | Octal PSRAM / Flash bus | Permanent hardware conflict |
| GPIO 19 | USB D- (if USB enabled) | USB communication |
| GPIO 20 | USB D+ (if USB enabled) | USB communication |

---

## Full GPIO Allocation Summary

| GPIO | Assigned To | Direction | Bus |
|---|---|---|---|
| 1 | SD_CS | OUT | HSPI |
| 2 | SD_MOSI | OUT | HSPI |
| 10 | TFT_CS | OUT | VSPI |
| 11 | TFT_MOSI | OUT | VSPI |
| 12 | TFT_SCLK | OUT | VSPI |
| 13 | TFT_DC | OUT | VSPI |
| 14 | TFT_RST | OUT | VSPI |
| 21 | TFT_BLK | OUT (PWM) | — |
| 33-37 | **RESERVED** | — | OPI Flash/PSRAM |
| 41 | SD_MISO | IN | HSPI |
| 42 | SD_SCLK | OUT | HSPI |
| 48 | STATUS_LED | OUT | — |

---

## Notes

- Both SPI buses operate independently and simultaneously via FreeRTOS on separate cores.
- The TFT and SD share no GPIO lines — no bus conflict possible.
- Pull-up resistors on SD MISO (GPIO 41) are strongly recommended for signal integrity at 25 MHz.
- GPIO 21 (TFT backlight) supports ledcWrite() for dimming control.
