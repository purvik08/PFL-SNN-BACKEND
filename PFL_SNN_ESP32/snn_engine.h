// snn_engine.h — PSRAM-Backed Vector-Optimized LIF SNN Inference Engine
#pragma once
#include <Arduino.h>
#include "config.h"

// Managed PSRAM Dynamic Tensor Buffer
struct PSRAMTensor {
  float* data;
  size_t numel;

  bool allocate(size_t n);
  void free_buf();
  void zero();
};

// PSRAM Cached Weights Structure
struct CachedWeights {
  int8_t* enc0_conv1; float enc0_conv1_s;
  int8_t* enc0_conv2; float enc0_conv2_s;
  int8_t* enc1_conv1; float enc1_conv1_s;
  int8_t* enc1_conv2; float enc1_conv2_s;
  int8_t* enc2_conv1; float enc2_conv1_s;
  int8_t* enc2_conv2; float enc2_conv2_s;
  int8_t* enc3_conv1; float enc3_conv1_s;
  int8_t* enc3_conv2; float enc3_conv2_s;

  int8_t* bot_conv1;  float bot_conv1_s;

  int8_t* dec0_up;    float dec0_up_s;
  int8_t* dec0_conv1; float dec0_conv1_s;
  int8_t* dec1_up;    float dec1_up_s;
  int8_t* dec1_conv1; float dec1_conv1_s;
  int8_t* dec2_up;    float dec2_up_s;
  int8_t* dec2_conv1; float dec2_conv1_s;
  int8_t* dec3_up;    float dec3_up_s;
  int8_t* dec3_conv1; float dec3_conv1_s;

  int8_t* out_conv;   float out_conv_s;
};

// Per-Layer LIF Neuron Membrane Potential State
struct LIFNeuronState {
  PSRAMTensor v_mem;  // Membrane potential V_m(t)
  void reset();
};

// Inference Metric Summary
struct SNNResult {
  float    change_percentage;   // 0.0% to 100.0%
  int      changed_pixels;
  int      total_pixels;
  float    mean_confidence;
  uint32_t inference_time_ms;
  bool     skipped_sparse;      // True if skipped via coarse diff check
  bool     valid;
};

class SNNEngine {
public:
  SNNEngine();
  ~SNNEngine();

  bool begin();
  void end();

  // Core Inference Call
  SNNResult infer(const float* img_t1,
                  const float* img_t2,
                  float threshold = SNN_CHANGE_THRESH);

  // Return pointer to PSRAM 1-byte per pixel change mask (0 or 1)
  const uint8_t* getChangeMask() const { return _changeMask; }

private:
  bool _initialized;
  CachedWeights _w;

  // PSRAM Feature Buffers
  PSRAMTensor _t1Skips[4];
  PSRAMTensor _t2Skips[4];
  PSRAMTensor _diffSkips[4];
  PSRAMTensor _bottleneck;
  PSRAMTensor _spikeAccumulator;
  uint8_t*    _changeMask;

  // LIF Membrane States (2 per decoder block x 4 blocks = 8 states)
  LIFNeuronState _lifStates[8];

  // Helper Methods
  bool _cacheWeightsToPSRAM();
  void _freeCachedWeights();
  bool _checkTileActivity(const float* t1, const float* t2, int total_px, float& mean_diff);

  // Vector / SIMD Accelerated Math Primitives
  void _conv2d_simd(const float* in, int iC, int iH, int iW,
                    float* out, int oC,
                    const int8_t* w_psram, float w_scale,
                    int kH = 3, int kW = 3, int pad = 1, int stride = 1);

  void _convTranspose2d_simd(const float* in, int iC, int iH, int iW,
                             float* out, int oC,
                             const int8_t* w_psram, float w_scale);

  void _batchNorm(float* x, int C, int HW,
                  const float* gamma, const float* beta,
                  const float* mean, const float* var, float eps = 1e-5f);

  void _relu(float* x, size_t n);
  void _maxpool2x2(const float* in, float* out, int C, int iH, int iW);
  void _lifStep(const float* current_in, float* mem, float* spikes_out,
                int n, float beta = SNN_BETA, float vth = SNN_VTH);
  void _sigmoid(float* x, size_t n);
  void _absDiff(const float* a, const float* b, float* out, size_t n);
  void _softmax2ch(const float* in, float* out, size_t pixelCount);
  void _encodeSingle(const float* img, PSRAMTensor skips[4], float* bot_out);
};

extern SNNEngine snnEngine;
