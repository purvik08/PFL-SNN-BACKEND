// config.h — GeoGuard ESP32-S3 Edge Node System Configuration
#pragma once
#include <Arduino.h>

// ============================================================================
// 1. HARDWARE & PERIPHERAL PIN MAPPING (ESP32-S3 N16R8)
// CRITICAL PIN GUARDRAIL: GPIO 33, 34, 35, 36, 37 ARE PERMANENTLY RESERVED
// FOR OCTAL SPI FLASH & PSRAM. DO NOT ASSIGN THEM TO ANY PERIPHERALS!
// ============================================================================

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

// ============================================================================
// 2. NETWORK & SERVER CONFIGURATION
// ============================================================================
#define WIFI_SSID "prajapati 4G"
#define WIFI_PASSWORD "medhansh02"
#define HTTP_PORT 80
#define FTP_PORT 21
#define FTP_USER "geoguard"
#define FTP_PASS "geoguard123"

#define GEO_API_URL "http://ip-api.com/json/"
#define GEO_TIMEOUT_MS 5000

// ============================================================================
// 3. EMBEDDED SNN HYPERPARAMETERS & TILING
// ============================================================================
#define SNN_PATCH_W 128 // Patch width (pixels)
#define SNN_PATCH_H 128 // Patch height (pixels)
#define SNN_NUM_STEPS 5 // LIF timesteps (T=5 for ultra-fast accumulation)
#define SNN_BETA 0.9f   // Membrane decay factor
#define SNN_VTH 0.5f    // LIF Firing Threshold
#define SNN_CHANGE_THRESH 0.5f // Softmax change decision boundary

// Coarse Difference Sparse Skipping
#define SPARSE_DIFF_ENABLE 1
#define SPARSE_DIFF_THRESH                                                     \
  0.035f // Skip SNN evaluation if mean |T2 - T1| < 3.5%

// Embedded Layer Channels
#define ENC_CH0 8
#define ENC_CH1 16
#define ENC_CH2 32
#define ENC_CH3 64
#define BOTTLENECK_CH 128

// ============================================================================
// 4. STORAGE & FILE PATHS
// ============================================================================
#define SD_MOUNT_POINT "/sdcard"
#define REPORTS_DIR "/reports"
#define MAX_REPORTS 100

// ============================================================================
// 5. FREERTOS TASK SCHEDULING
// ============================================================================
#define TASK_NET_CORE 0 // Wi-Fi, WebServer, FTP on Core 0
#define TASK_INF_CORE 1 // SNN Inference, SD I/O, TFT on Core 1

#define NET_TASK_STACK 8192
#define INF_TASK_STACK 16384
