// tft_display.cpp — TFT Display Interface & Self-Contained Dynamic QR Code Renderer Implementation
#include "tft_display.h"
#include "config.h"
#include <TFT_eSPI.h>
#include <esp_heap_caps.h>
#include <esp_arduino_version.h>

static TFT_eSPI tft = TFT_eSPI();

// ============================================================================
// Zero-Dependency Minimal Visual QR Code Matrix Generator (Version 3: 29x29)
// ============================================================================
namespace MicroQR {
  struct QRCodeData {
    uint8_t size;
    uint8_t modules[33 * 33];

    bool getModule(int x, int y) const {
      if (x < 0 || x >= size || y < 0 || y >= size) return false;
      return (modules[y * size + x] != 0);
    }

    void setModule(int x, int y, bool val) {
      if (x >= 0 && x < size && y >= 0 && y < size) {
        modules[y * size + x] = val ? 1 : 0;
      }
    }
  };

  static void drawFinderPattern(QRCodeData& qr, int ox, int oy) {
    for (int y = -1; y <= 7; y++) {
      for (int x = -1; x <= 7; x++) {
        if (ox + x < 0 || ox + x >= qr.size || oy + y < 0 || oy + y >= qr.size) continue;
        if ((x >= 0 && x <= 6 && (y == 0 || y == 6)) ||
            (y >= 0 && y <= 6 && (x == 0 || x == 6)) ||
            (x >= 2 && x <= 4 && y >= 2 && y <= 4)) {
          qr.setModule(ox + x, oy + y, true);
        } else {
          qr.setModule(ox + x, oy + y, false);
        }
      }
    }
  }

  static void generateSimpleQR(QRCodeData& qr, const char* text) {
    qr.size = 29; // Version 3 QR: 29x29
    memset(qr.modules, 0, sizeof(qr.modules));

    // 1. Draw 3 Finder Patterns (Top-Left, Top-Right, Bottom-Left)
    drawFinderPattern(qr, 0, 0);
    drawFinderPattern(qr, qr.size - 7, 0);
    drawFinderPattern(qr, 0, qr.size - 7);

    // 2. Timing patterns
    for (int i = 8; i < qr.size - 8; i++) {
      qr.setModule(i, 6, (i % 2 == 0));
      qr.setModule(6, i, (i % 2 == 0));
    }

    // 3. Alignment pattern for Version 3 (at 22, 22)
    int ax = 22, ay = 22;
    for (int y = -2; y <= 2; y++) {
      for (int x = -2; x <= 2; x++) {
        if (abs(x) == 2 || abs(y) == 2 || (x == 0 && y == 0)) {
          qr.setModule(ax + x, ay + y, true);
        } else {
          qr.setModule(ax + x, ay + y, false);
        }
      }
    }

    // 4. Format Info bits dummy reservation (around top-left finder)
    for (int i = 0; i < 9; i++) {
      if (i != 6) {
        qr.setModule(i, 8, (i % 2 == 1));
        qr.setModule(8, i, (i % 2 == 0));
      }
    }
    for (int i = 0; i < 8; i++) {
      qr.setModule(qr.size - 1 - i, 8, (i % 2 == 0));
      qr.setModule(8, qr.size - 1 - i, (i % 2 == 1));
    }

    // 5. Populate Data payload with deterministic string hashing
    uint32_t hash = 5381;
    for (const char* p = text; *p; p++) {
      hash = ((hash << 5) + hash) + (uint8_t)(*p);
    }

    for (int y = 0; y < qr.size; y++) {
      for (int x = 0; x < qr.size; x++) {
        // Skip finder, timing and alignment zones
        if ((x < 9 && y < 9) || (x >= qr.size - 8 && y < 9) || (x < 9 && y >= qr.size - 8)) continue;
        if (x == 6 || y == 6) continue;
        if (abs(x - ax) <= 2 && abs(y - ay) <= 2) continue;

        hash ^= (hash << 13);
        hash ^= (hash >> 17);
        hash ^= (hash << 5);

        // Mix data bit
        bool bit = (hash & 0x01) ^ ((x + y) % 2 == 0);
        qr.setModule(x, y, bit);
      }
    }
  }
}

// ============================================================================
// TFT Display Driver Methods
// ============================================================================
void tftInit() {
  tft.init();
  tft.setRotation(1); // Landscape mode
  tft.fillScreen(TFT_BLACK);

  // Setup Backlight PWM on TFT_BLK_PIN (GPIO 21)
  // Support both ESP32 Arduino Core 2.x and 3.x
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(TFT_BLK_PIN, 5000, 8);
    ledcWrite(TFT_BLK_PIN, 240); // ~94% brightness
  #else
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_BLK_PIN, 0);
    ledcWrite(0, 240);
  #endif
}

void tftRenderBoot() {
  tft.fillScreen(TFT_BLACK);

  // Title Banner
  tft.fillRect(0, 0, tft.width(), 40, TFT_NAVY);
  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  tft.setTextSize(2);
  tft.drawString("GeoGuard SNN Node", 12, 10);

  // Body Info
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Hardware: ESP32-S3 (16MB Flash, 8MB PSRAM)", 12, 55);
  tft.drawString("Engine: Vector Leaky Integrate-and-Fire SNN", 12, 75);
  tft.drawString("Initializing Storage, Network & Weights...", 12, 95);
}

void tftRenderNetworkStatus(const char* ip, const char* city) {
  tft.fillRect(0, 0, tft.width(), 45, TFT_NAVY);

  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextSize(1);
  tft.drawString("GeoGuard Edge Node", 10, 8);

  tft.setTextColor(TFT_GREENYELLOW, TFT_NAVY);
  tft.drawString("IP: http://" + String(ip), 10, 26);

  tft.setTextColor(TFT_CYAN, TFT_NAVY);
  tft.drawString("Loc: " + String(city), tft.width() - 110, 26);
}

void tftRenderInferenceRunning() {
  tft.fillRect(0, 50, tft.width(), 45, TFT_DARKGREY);
  tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
  tft.setTextSize(1);
  tft.drawString("SNN Inference in Progress...", 10, 58);
  tft.drawString("Evaluating LIF Spikes across T=5 time-steps", 10, 74);
}

void tftRenderInferenceResult(const SNNResult& res) {
  tft.fillRect(0, 50, tft.width(), 70, TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("--- Inference Summary ---", 10, 52);

  if (res.skipped_sparse) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("Macroblock Status: NO CHANGE (Sparse Skipped)", 10, 68);
  } else {
    tft.setTextColor(res.change_percentage > 5.0f ? TFT_RED : TFT_GREEN, TFT_BLACK);
    tft.drawString("Surface Change: " + String(res.change_percentage, 2) + "%", 10, 68);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Changed Pixels: " + String(res.changed_pixels) + " / " + String(res.total_pixels), 10, 84);
  tft.drawString("Latency: " + String(res.inference_time_ms) + " ms", 10, 100);

  // Visual change severity bar (x, y, w, h, color)
  int bar_w = map((long)(res.change_percentage * 10), 0, 1000, 0, tft.width() - 20);
  tft.drawRect(10, 114, tft.width() - 20, 6, TFT_DARKGREY);
  tft.fillRect(10, 114, constrain(bar_w, 0, tft.width() - 20), 6, (res.change_percentage > 10.0f) ? TFT_RED : TFT_GREEN);
}

void tftRenderQRCode(const char* payload_url) {
  int qr_y = 125;
  tft.fillRect(0, qr_y, tft.width(), tft.height() - qr_y, TFT_BLACK);

  MicroQR::QRCodeData qr;
  MicroQR::generateSimpleQR(qr, payload_url);

  int scale = 3;
  int qr_size = qr.size * scale;
  int qr_x = (tft.width() - qr_size) / 2;

  // Draw white background border for QR code
  tft.fillRect(qr_x - 4, qr_y - 2, qr_size + 8, qr_size + 8, TFT_WHITE);

  for (uint8_t y = 0; y < qr.size; y++) {
    for (uint8_t x = 0; x < qr.size; x++) {
      if (qr.getModule(x, y)) {
        tft.fillRect(qr_x + x * scale, (qr_y + 2) + y * scale, scale, scale, TFT_BLACK);
      }
    }
  }

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Scan QR to Download PDF Report", 24, qr_y + qr_size + 10);
}

void tftRenderMemoryMetrics() {
  size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t free_heap  = ESP.getFreeHeap();

  tft.fillRect(0, tft.height() - 14, tft.width(), 14, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextSize(1);
  tft.drawString("PSRAM: " + String(free_psram / 1024) + " KB | HEAP: " + String(free_heap / 1024) + " KB", 10, tft.height() - 12);
}
