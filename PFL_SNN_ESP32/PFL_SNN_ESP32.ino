// ============================================================================
// PFL_SNN_ESP32.ino — Standalone Embedded SNN Change Detection & Reporting Node
// Hardware: ESP32-S3 N16R8 (16MB Flash, 8MB Octal PSRAM)
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "snn_engine.h"
#include "pdf_generator.h"
#include "web_server.h"
#include "ftp_server.h"
#include "tft_display.h"
#include "geo_locator.h"

// ── Global System State ──────────────────────────────────────────────────────
static DeviceGeoTag g_deviceLocation;
static char         g_ipString[24] = "0.0.0.0";
static int          g_reportCounter = 0;

// ── FreeRTOS Task: Core 1 (SNN Inference & SD Card File Operations) ──────────
void inferenceTaskCore1(void* pvParameters) {
  Serial.printf("[TASK] Inference Task running on Core %d\n", xPortGetCoreID());

  for (;;) {
    if (g_inferenceRequested && !g_inferenceRunning) {
      g_inferenceRunning = true;
      g_inferenceRequested = false;

      // Update TFT
      tftRenderInferenceRunning();

      Serial.println("[MAIN] Executing SNN Change Detection pass...");
      SNNResult result = snnEngine.infer(g_bufferT1, g_bufferT2, g_inferenceThreshold);

      if (result.valid) {
        g_reportCounter++;

        // Format Report Filename
        char pdf_path[64];
        snprintf(pdf_path, sizeof(pdf_path), "%s/report_%03d.pdf", REPORTS_DIR, g_reportCounter);

        // Build Report Metadata
        PDFReportMetadata meta{};
        strlcpy(meta.filepath, pdf_path, sizeof(meta.filepath));
        meta.report_id         = g_reportCounter;
        meta.geo.lat           = g_deviceLocation.lat;
        meta.geo.lon           = g_deviceLocation.lon;
        strlcpy(meta.geo.city,    g_deviceLocation.city,    sizeof(meta.geo.city));
        strlcpy(meta.geo.country, g_deviceLocation.country, sizeof(meta.geo.country));

        meta.changed_pixels    = result.changed_pixels;
        meta.total_pixels      = result.total_pixels;
        meta.change_percentage = result.change_percentage;
        meta.threshold         = g_inferenceThreshold;
        meta.inference_ms      = result.inference_time_ms;
        meta.sparse_skipped    = result.skipped_sparse;

        snprintf(meta.timestamp_str, sizeof(meta.timestamp_str), "2026-09-17 23:50 UTC");

        // Generate PDF directly to SD Card
        bool pdf_ok = pdfGenerator.generate(meta, snnEngine.getChangeMask(), SNN_PATCH_W, SNN_PATCH_H);

        if (pdf_ok) {
          webServerSetLastReport(pdf_path);

          // Build download URL for QR code (HTTP or FTP)
          char download_url[128];
          snprintf(download_url, sizeof(download_url), "http://%s/report/latest", g_ipString);

          // Update Display
          tftRenderInferenceResult(result);
          tftRenderQRCode(download_url);
          tftRenderMemoryMetrics();
        }
      }

      g_inferenceRunning = false;
    }

    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

// ── FreeRTOS Task: Core 0 (Network, AsyncWebServer & FTP Engine) ──────────────
void networkTaskCore0(void* pvParameters) {
  Serial.printf("[TASK] Network Task running on Core %d\n", xPortGetCoreID());

  for (;;) {
    // Process FTP non-blocking event loop
    ftpServerProcess();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ── Arduino Setup ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=======================================================");
  Serial.println("  GeoGuard Autonomous Embedded SNN Node (ESP32-S3 N16R8) ");
  Serial.println("=======================================================");

  // 1. Validate PSRAM (8MB Octal PSRAM)
  if (!psramFound()) {
    Serial.println("[HAL] FATAL: PSRAM not found! Ensure OPI PSRAM is enabled in tools.");
    while (1) { delay(1000); }
  }
  Serial.printf("[HAL] Total PSRAM: %u MB | Free: %u KB\n",
                (unsigned)(ESP.getPsramSize() / (1024 * 1024)),
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

  // 2. Initialize TFT Display (SPI Bus 1)
  tftInit();
  tftRenderBoot();

  // 3. Initialize Flash FileSystem (LittleFS)
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS Mount Failed (Using inline fallback UI)");
  } else {
    Serial.printf("[FS] LittleFS Mounted. Free space: %u KB\n",
                  (unsigned)((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024));
  }

  // 4. Initialize MicroSD Card (SPI Bus 2)
  SPIClass spiSD(HSPI);
  spiSD.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, spiSD, 25000000)) {
    Serial.println("[SD] ERROR: MicroSD Card Mount Failed! Check wiring & card format (FAT32).");
  } else {
    Serial.printf("[SD] MicroSD Card Mounted. Total: %llu MB\n", SD.cardSize() / (1024 * 1024));
    SD.mkdir(REPORTS_DIR);
  }

  // 5. Connect to Wi-Fi
  Serial.printf("[NET] Connecting to Wi-Fi SSID: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int wifi_retries = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_retries < 25) {
    delay(400);
    Serial.print(".");
    wifi_retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    strlcpy(g_ipString, WiFi.localIP().toString().c_str(), sizeof(g_ipString));
    Serial.printf("\n[NET] Wi-Fi Connected! Local IP: %s\n", g_ipString);
  } else {
    Serial.println("\n[NET] Wi-Fi Connection Timeout. Starting in AP/Offline Mode.");
    WiFi.softAP("GeoGuard_SNN_Node", "12345678");
    strlcpy(g_ipString, WiFi.softAPIP().toString().c_str(), sizeof(g_ipString));
    Serial.printf("[NET] SoftAP IP: %s\n", g_ipString);
  }

  // 6. Fetch IP Geolocation
  fetchDeviceLocation(g_deviceLocation);
  tftRenderNetworkStatus(g_ipString, g_deviceLocation.city);
  tftRenderMemoryMetrics();

  // 7. Initialize Vector SNN Inference Engine
  if (!snnEngine.begin()) {
    Serial.println("[SNN] FATAL: SNN Engine initialization failed!");
    while (1) { delay(1000); }
  }

  // 8. Start Web Server & FTP Services
  webServerInit();
  ftpServerInit();

  // 9. Launch FreeRTOS Tasks on Dedicated Cores
  xTaskCreatePinnedToCore(
    networkTaskCore0,
    "NetTask_Core0",
    NET_TASK_STACK,
    nullptr,
    1,
    nullptr,
    TASK_NET_CORE
  );

  xTaskCreatePinnedToCore(
    inferenceTaskCore1,
    "InfTask_Core1",
    INF_TASK_STACK,
    nullptr,
    2,
    nullptr,
    TASK_INF_CORE
  );

  Serial.println("[SYS] System Initialization Complete. Ready for Ingestion.");
}

// ── Main Loop ────────────────────────────────────────────────────────────────
void loop() {
  // Periodically refresh memory metrics on TFT
  static uint32_t last_mem_refresh = 0;
  if (millis() - last_mem_refresh > 8000) {
    tftRenderMemoryMetrics();
    last_mem_refresh = millis();
  }
  delay(200);
}
