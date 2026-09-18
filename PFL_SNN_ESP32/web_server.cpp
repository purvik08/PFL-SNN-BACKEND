// web_server.cpp — Local Web Server & HTTP Map Ingestion Handlers Implementation
#include "web_server.h"
#include "config.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

static AsyncWebServer server(HTTP_PORT);
static char _latestReport[64] = "";

float* g_bufferT1 = nullptr;
float* g_bufferT2 = nullptr;

volatile bool  g_inferenceRequested = false;
volatile bool  g_inferenceRunning   = false;
volatile float g_inferenceThreshold = SNN_CHANGE_THRESH;

void webServerSetLastReport(const char* filepath) {
  strlcpy(_latestReport, filepath, sizeof(_latestReport));
}

// Minimal fallback embedded UI HTML
static const char INDEX_HTML_FALLBACK[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>GeoGuard ESP32-S3 SNN Edge Node</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 20px; }
    .card { background: #1e293b; border-radius: 12px; padding: 24px; max-width: 640px; margin: 0 auto; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.3); }
    h1 { color: #38bdf8; font-size: 24px; margin-top: 0; }
    .drop-zone { border: 2px dashed #475569; border-radius: 8px; padding: 20px; text-align: center; margin-bottom: 16px; cursor: pointer; }
    .drop-zone:hover { border-color: #38bdf8; }
    button { background: #0284c7; color: white; border: none; padding: 12px 24px; border-radius: 6px; font-weight: bold; cursor: pointer; width: 100%; font-size: 16px; }
    button:hover { background: #0369a1; }
    .status-box { margin-top: 20px; padding: 12px; background: #0f172a; border-radius: 6px; font-family: monospace; font-size: 14px; }
    .stat-row { display: flex; justify-content: space-between; margin-bottom: 6px; }
    .badge { background: #22c55e; color: #052e16; padding: 2px 8px; border-radius: 4px; font-weight: bold; }
  </style>
</head>
<body>
  <div class="card">
    <h1>🛰️ GeoGuard SNN Edge Node</h1>
    <p>Upload aligned map image patches (128x128 grayscale / raw float)</p>

    <div class="drop-zone" onclick="document.getElementById('f1').click()">
      <strong>Temporal 1 (Before Image)</strong>
      <input type="file" id="f1" style="display:none" onchange="uploadFile('t1', this.files[0])">
      <div id="t1-name" style="color: #94a3b8; font-size: 12px; margin-top: 4px;">Select Image</div>
    </div>

    <div class="drop-zone" onclick="document.getElementById('f2').click()">
      <strong>Temporal 2 (After Image)</strong>
      <input type="file" id="f2" style="display:none" onchange="uploadFile('t2', this.files[0])">
      <div id="t2-name" style="color: #94a3b8; font-size: 12px; margin-top: 4px;">Select Image</div>
    </div>

    <div style="margin-bottom: 16px;">
      <label>Confidence Threshold: <span id="thresh-val">0.50</span></label>
      <input type="range" id="thresh" min="0.1" max="0.9" step="0.05" value="0.50" style="width:100%" oninput="document.getElementById('thresh-val').innerText=this.value">
    </div>

    <button onclick="triggerInference()">⚡ Run SNN Change Detection</button>

    <div class="status-box" id="status">
      <div class="stat-row"><span>Status:</span><span class="badge" id="s-state">Ready</span></div>
      <div class="stat-row"><span>Free PSRAM:</span><span id="s-psram">-- KB</span></div>
      <div class="stat-row"><span>Latest PDF:</span><span id="s-pdf">None</span></div>
    </div>
  </div>

  <script>
    async function uploadFile(slot, file) {
      if(!file) return;
      document.getElementById(slot+'-name').innerText = file.name + ' (' + file.size + ' B)';
      const buf = await file.arrayBuffer();
      await fetch('/upload/' + slot, { method: 'POST', body: buf });
    }

    async function triggerInference() {
      const th = document.getElementById('thresh').value;
      document.getElementById('s-state').innerText = "Processing...";
      await fetch('/infer?thresh=' + th, { method: 'POST' });
      pollStatus();
    }

    async function pollStatus() {
      const res = await fetch('/status');
      const data = await res.json();
      document.getElementById('s-psram').innerText = Math.round(data.free_psram / 1024) + ' KB';
      document.getElementById('s-state').innerText = data.inference_running ? "Inferring..." : "Ready";
      if(data.last_report) {
        document.getElementById('s-pdf').innerHTML = '<a href="/report/latest" style="color:#38bdf8" target="_blank">' + data.last_report + '</a>';
      }
      if(data.inference_running) setTimeout(pollStatus, 1000);
    }
    setInterval(pollStatus, 5000);
    pollStatus();
  </script>
</body>
</html>
)rawliteral";

void webServerInit() {
  // Allocate PSRAM input frame buffers
  size_t frame_bytes = (size_t)SNN_PATCH_W * SNN_PATCH_H * sizeof(float);
  g_bufferT1 = (float*)heap_caps_malloc(frame_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  g_bufferT2 = (float*)heap_caps_malloc(frame_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  if (!g_bufferT1 || !g_bufferT2) {
    Serial.println("[WEB] ERROR: Failed to allocate T1/T2 PSRAM frame buffers!");
  } else {
    memset(g_bufferT1, 0, frame_bytes);
    memset(g_bufferT2, 0, frame_bytes);
  }

  // 1. Serve Root Page (LittleFS or Embedded HTML Fallback)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send_P(200, "text/html", INDEX_HTML_FALLBACK);
    }
  });

  // 2. Upload T1 Image Patch (Raw 8-bit or 32-bit Float)
  server.on("/upload/t1", HTTP_POST, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", "{\"status\":\"success\",\"slot\":\"t1\"}");
  }, nullptr, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (!g_bufferT1) return;
    size_t max_bytes = (size_t)SNN_PATCH_W * SNN_PATCH_H * sizeof(float);

    // If incoming format is raw byte grayscale (128x128 = 16384 bytes), convert on-the-fly to float [0, 1]
    if (total == SNN_PATCH_W * SNN_PATCH_H) {
      for (size_t i = 0; i < len; i++) {
        if (index + i < SNN_PATCH_W * SNN_PATCH_H) {
          g_bufferT1[index + i] = (float)data[i] / 255.0f;
        }
      }
    } else {
      // Direct raw float buffer copy
      if (index + len <= max_bytes) {
        memcpy(((uint8_t*)g_bufferT1) + index, data, len);
      }
    }
  });

  // 3. Upload T2 Image Patch
  server.on("/upload/t2", HTTP_POST, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", "{\"status\":\"success\",\"slot\":\"t2\"}");
  }, nullptr, [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    if (!g_bufferT2) return;
    size_t max_bytes = (size_t)SNN_PATCH_W * SNN_PATCH_H * sizeof(float);

    if (total == SNN_PATCH_W * SNN_PATCH_H) {
      for (size_t i = 0; i < len; i++) {
        if (index + i < SNN_PATCH_W * SNN_PATCH_H) {
          g_bufferT2[index + i] = (float)data[i] / 255.0f;
        }
      }
    } else {
      if (index + len <= max_bytes) {
        memcpy(((uint8_t*)g_bufferT2) + index, data, len);
      }
    }
  });

  // 4. Trigger SNN Inference Asynchronously on Core 1
  server.on("/infer", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (g_inferenceRunning) {
      request->send(409, "application/json", "{\"error\":\"Inference already in progress\"}");
      return;
    }

    if (request->hasParam("thresh")) {
      g_inferenceThreshold = request->getParam("thresh")->value().toFloat();
    } else {
      g_inferenceThreshold = SNN_CHANGE_THRESH;
    }

    g_inferenceRequested = true;
    request->send(202, "application/json", "{\"status\":\"queued\",\"threshold\": " + String(g_inferenceThreshold) + "}");
  });

  // 5. System Status Telemetry
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["inference_running"] = (bool)g_inferenceRunning;
    doc["free_psram"]        = (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    doc["free_internal_ram"] = (int)ESP.getFreeHeap();
    doc["last_report"]       = _latestReport;

    char res_buf[256];
    serializeJson(doc, res_buf, sizeof(res_buf));
    request->send(200, "application/json", res_buf);
  });

  // 6. Direct PDF Download Route
  server.on("/report/latest", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (_latestReport[0] == '\0' || !SD.exists(_latestReport)) {
      request->send(404, "text/plain", "No generated report found on SD card.");
    } else {
      request->send(SD, _latestReport, "application/pdf", true);
    }
  });

  server.begin();
  Serial.printf("[WEB] HTTP Server listening on port %d\n", HTTP_PORT);
}
