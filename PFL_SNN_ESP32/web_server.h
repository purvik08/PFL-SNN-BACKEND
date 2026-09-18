// web_server.h — Local Web Server & HTTP Map Ingestion Handlers
#pragma once
#include <Arduino.h>

void webServerInit();
void webServerSetLastReport(const char* filepath);

// Global PSRAM input frame buffers for T1 & T2
extern float* g_bufferT1;
extern float* g_bufferT2;

// Inter-core execution flags
extern volatile bool g_inferenceRequested;
extern volatile bool g_inferenceRunning;
extern volatile float g_inferenceThreshold;
