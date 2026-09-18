// tft_display.h — TFT Display Interface & Dynamic QR Code Renderer
#pragma once
#include <Arduino.h>
#include "snn_engine.h"

void tftInit();
void tftRenderBoot();
void tftRenderNetworkStatus(const char* ip, const char* city);
void tftRenderInferenceRunning();
void tftRenderInferenceResult(const SNNResult& res);
void tftRenderQRCode(const char* payload_url);
void tftRenderMemoryMetrics();
