// snn_engine.cpp — PSRAM-Backed Vector-Optimized LIF SNN Inference Implementation
#include "snn_engine.h"
#include "model_weights.h"
#include <esp_heap_caps.h>
#include <cmath>
#include <cstring>

SNNEngine snnEngine;

// ── PSRAM Tensor Implementation ──────────────────────────────────────────────
bool PSRAMTensor::allocate(size_t n) {
  numel = n;
  data = (float*)heap_caps_malloc(n * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!data) {
    Serial.printf("[PSRAM] Failed to allocate tensor size: %u bytes\n", (unsigned)(n * sizeof(float)));
    return false;
  }
  return true;
}

void PSRAMTensor::free_buf() {
  if (data) {
    heap_caps_free(data);
    data = nullptr;
    numel = 0;
  }
}

void PSRAMTensor::zero() {
  if (data && numel > 0) {
    memset(data, 0, numel * sizeof(float));
  }
}

void LIFNeuronState::reset() {
  v_mem.zero();
}

// ── SNNEngine Constructor / Destructor ───────────────────────────────────────
SNNEngine::SNNEngine() : _initialized(false), _changeMask(nullptr) {
  memset(&_w, 0, sizeof(_w));
}

SNNEngine::~SNNEngine() {
  end();
}

// ── PSRAM Weight Caching (Eliminates Slow SPI Flash Reads) ────────────────────
static int8_t* allocAndCopyProgmem(const int8_t* src_progmem, size_t count) {
  int8_t* dst = (int8_t*)heap_caps_malloc(count * sizeof(int8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (dst) {
    memcpy_P(dst, src_progmem, count);
  }
  return dst;
}

bool SNNEngine::_cacheWeightsToPSRAM() {
  _w.enc0_conv1 = allocAndCopyProgmem(enc0_conv1_w, sizeof(enc0_conv1_w));
  _w.enc0_conv1_s = enc0_conv1_scale;
  _w.enc0_conv2 = allocAndCopyProgmem(enc0_conv2_w, sizeof(enc0_conv2_w));
  _w.enc0_conv2_s = enc0_conv2_scale;

  _w.enc1_conv1 = allocAndCopyProgmem(enc1_conv1_w, sizeof(enc1_conv1_w));
  _w.enc1_conv1_s = enc1_conv1_scale;
  _w.enc1_conv2 = allocAndCopyProgmem(enc1_conv2_w, sizeof(enc1_conv2_w));
  _w.enc1_conv2_s = enc1_conv2_scale;

  _w.enc2_conv1 = allocAndCopyProgmem(enc2_conv1_w, sizeof(enc2_conv1_w));
  _w.enc2_conv1_s = enc2_conv1_scale;
  _w.enc2_conv2 = allocAndCopyProgmem(enc2_conv2_w, sizeof(enc2_conv2_w));
  _w.enc2_conv2_s = enc2_conv2_scale;

  _w.enc3_conv1 = allocAndCopyProgmem(enc3_conv1_w, sizeof(enc3_conv1_w));
  _w.enc3_conv1_s = enc3_conv1_scale;
  _w.enc3_conv2 = allocAndCopyProgmem(enc3_conv2_w, sizeof(enc3_conv2_w));
  _w.enc3_conv2_s = enc3_conv2_scale;

  _w.bot_conv1  = allocAndCopyProgmem(bot_conv1_w, sizeof(bot_conv1_w));
  _w.bot_conv1_s  = bot_conv1_scale;

  _w.dec0_up    = allocAndCopyProgmem(dec0_up_w, sizeof(dec0_up_w));
  _w.dec0_up_s    = dec0_up_scale;
  _w.dec0_conv1 = allocAndCopyProgmem(dec0_conv1_w, sizeof(dec0_conv1_w));
  _w.dec0_conv1_s = dec0_conv1_scale;

  _w.dec1_up    = allocAndCopyProgmem(dec1_up_w, sizeof(dec1_up_w));
  _w.dec1_up_s    = dec1_up_scale;
  _w.dec1_conv1 = allocAndCopyProgmem(dec1_conv1_w, sizeof(dec1_conv1_w));
  _w.dec1_conv1_s = dec1_conv1_scale;

  _w.dec2_up    = allocAndCopyProgmem(dec2_up_w, sizeof(dec2_up_w));
  _w.dec2_up_s    = dec2_up_scale;
  _w.dec2_conv1 = allocAndCopyProgmem(dec2_conv1_w, sizeof(dec2_conv1_w));
  _w.dec2_conv1_s = dec2_conv1_scale;

  _w.dec3_up    = allocAndCopyProgmem(dec3_up_w, sizeof(dec3_up_w));
  _w.dec3_up_s    = dec3_up_scale;
  _w.dec3_conv1 = allocAndCopyProgmem(dec3_conv1_w, sizeof(dec3_conv1_w));
  _w.dec3_conv1_s = dec3_conv1_scale;

  _w.out_conv   = allocAndCopyProgmem(out_conv_w, sizeof(out_conv_w));
  _w.out_conv_s   = out_conv_scale;

  return (_w.enc0_conv1 && _w.bot_conv1 && _w.dec3_conv1 && _w.out_conv);
}

void SNNEngine::_freeCachedWeights() {
  if (_w.enc0_conv1) heap_caps_free(_w.enc0_conv1);
  if (_w.enc0_conv2) heap_caps_free(_w.enc0_conv2);
  if (_w.enc1_conv1) heap_caps_free(_w.enc1_conv1);
  if (_w.enc1_conv2) heap_caps_free(_w.enc1_conv2);
  if (_w.enc2_conv1) heap_caps_free(_w.enc2_conv1);
  if (_w.enc2_conv2) heap_caps_free(_w.enc2_conv2);
  if (_w.enc3_conv1) heap_caps_free(_w.enc3_conv1);
  if (_w.enc3_conv2) heap_caps_free(_w.enc3_conv2);
  if (_w.bot_conv1)  heap_caps_free(_w.bot_conv1);
  if (_w.dec0_up)    heap_caps_free(_w.dec0_up);
  if (_w.dec0_conv1) heap_caps_free(_w.dec0_conv1);
  if (_w.dec1_up)    heap_caps_free(_w.dec1_up);
  if (_w.dec1_conv1) heap_caps_free(_w.dec1_conv1);
  if (_w.dec2_up)    heap_caps_free(_w.dec2_up);
  if (_w.dec2_conv1) heap_caps_free(_w.dec2_conv1);
  if (_w.dec3_up)    heap_caps_free(_w.dec3_up);
  if (_w.dec3_conv1) heap_caps_free(_w.dec3_conv1);
  if (_w.out_conv)   heap_caps_free(_w.out_conv);
  memset(&_w, 0, sizeof(_w));
}

// ── Initialization ───────────────────────────────────────────────────────────
bool SNNEngine::begin() {
  if (_initialized) return true;

  const int H = SNN_PATCH_H;
  const int W = SNN_PATCH_W;
  const int ch[4] = {ENC_CH0, ENC_CH1, ENC_CH2, ENC_CH3};
  const int hs[5] = {H, H / 2, H / 4, H / 8, H / 16};
  const int ws[5] = {W, W / 2, W / 4, W / 8, W / 16};

  // 1. Cache weights in PSRAM
  if (!_cacheWeightsToPSRAM()) {
    Serial.println("[SNN] ERROR: Failed to cache model weights in PSRAM!");
    end();
    return false;
  }

  // 2. Allocate PSRAM skip feature buffers
  for (int i = 0; i < 4; i++) {
    size_t sz = (size_t)ch[i] * hs[i] * ws[i];
    if (!_t1Skips[i].allocate(sz) || !_t2Skips[i].allocate(sz) || !_diffSkips[i].allocate(sz)) {
      Serial.println("[SNN] ERROR: Skip buffer allocation failed!");
      end();
      return false;
    }
  }

  // 3. Bottleneck and Accumulator Tensors
  if (!_bottleneck.allocate((size_t)BOTTLENECK_CH * hs[4] * ws[4])) {
    end();
    return false;
  }
  if (!_spikeAccumulator.allocate(2UL * H * W)) {
    end();
    return false;
  }

  // 4. LIF Membrane Potential States
  for (int i = 0; i < 4; i++) {
    int out_ch = ch[3 - i];
    int cur_h  = hs[4 - i - 1];
    int cur_w  = ws[4 - i - 1];
    size_t mem_sz = (size_t)out_ch * cur_h * cur_w;

    if (!_lifStates[i * 2].v_mem.allocate(mem_sz) || !_lifStates[i * 2 + 1].v_mem.allocate(mem_sz)) {
      Serial.println("[SNN] ERROR: LIF state memory allocation failed!");
      end();
      return false;
    }
    _lifStates[i * 2].reset();
    _lifStates[i * 2 + 1].reset();
  }

  // 5. Binary Change Mask (1 byte per pixel)
  _changeMask = (uint8_t*)heap_caps_malloc((size_t)H * W, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!_changeMask) {
    end();
    return false;
  }
  memset(_changeMask, 0, H * W);

  _initialized = true;
  Serial.printf("[SNN] Engine Initialized. Free PSRAM: %u KB\n",
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  return true;
}

void SNNEngine::end() {
  _freeCachedWeights();
  for (int i = 0; i < 4; i++) {
    _t1Skips[i].free_buf();
    _t2Skips[i].free_buf();
    _diffSkips[i].free_buf();
  }
  _bottleneck.free_buf();
  _spikeAccumulator.free_buf();
  for (int i = 0; i < 8; i++) {
    _lifStates[i].v_mem.free_buf();
  }
  if (_changeMask) {
    heap_caps_free(_changeMask);
    _changeMask = nullptr;
  }
  _initialized = false;
}

// ── Math & Activation Primitives ─────────────────────────────────────────────
void SNNEngine::_relu(float* x, size_t n) {
  // Unrolled 4x for speed
  size_t i = 0;
  for (; i + 4 <= n; i += 4) {
    if (x[i]   < 0.0f) x[i]   = 0.0f;
    if (x[i+1] < 0.0f) x[i+1] = 0.0f;
    if (x[i+2] < 0.0f) x[i+2] = 0.0f;
    if (x[i+3] < 0.0f) x[i+3] = 0.0f;
  }
  for (; i < n; i++) {
    if (x[i] < 0.0f) x[i] = 0.0f;
  }
}

void SNNEngine::_sigmoid(float* x, size_t n) {
  for (size_t i = 0; i < n; i++) {
    x[i] = 1.0f / (1.0f + expf(-x[i]));
  }
}

void SNNEngine::_absDiff(const float* a, const float* b, float* out, size_t n) {
  size_t i = 0;
  for (; i + 4 <= n; i += 4) {
    out[i]   = fabsf(a[i]   - b[i]);
    out[i+1] = fabsf(a[i+1] - b[i+1]);
    out[i+2] = fabsf(a[i+2] - b[i+2]);
    out[i+3] = fabsf(a[i+3] - b[i+3]);
  }
  for (; i < n; i++) {
    out[i] = fabsf(a[i] - b[i]);
  }
}

void SNNEngine::_batchNorm(float* x, int C, int HW,
                           const float* gamma, const float* beta,
                           const float* mean, const float* var, float eps) {
  for (int c = 0; c < C; c++) {
    float g = pgm_read_float(&gamma[c]);
    float b = pgm_read_float(&beta[c]);
    float m = pgm_read_float(&mean[c]);
    float v = pgm_read_float(&var[c]);
    float inv_std = 1.0f / sqrtf(v + eps);
    float scale = g * inv_std;
    float offset = b - m * scale;

    float* ptr = x + c * HW;
    for (int i = 0; i < HW; i++) {
      ptr[i] = ptr[i] * scale + offset;
    }
  }
}

void SNNEngine::_maxpool2x2(const float* in, float* out, int C, int iH, int iW) {
  int oH = iH / 2;
  int oW = iW / 2;
  for (int c = 0; c < C; c++) {
    const float* in_plane = in + c * iH * iW;
    float* out_plane = out + c * oH * oW;
    for (int h = 0; h < oH; h++) {
      for (int w = 0; w < oW; w++) {
        int idx = (h * 2) * iW + (w * 2);
        float m = in_plane[idx];
        float v1 = in_plane[idx + 1];
        float v2 = in_plane[idx + iW];
        float v3 = in_plane[idx + iW + 1];
        if (v1 > m) m = v1;
        if (v2 > m) m = v2;
        if (v3 > m) m = v3;
        out_plane[h * oW + w] = m;
      }
    }
  }
}

// ── Vectorized 2D Convolution using PSRAM Cached Weights ─────────────────────
void SNNEngine::_conv2d_simd(const float* in, int iC, int iH, int iW,
                             float* out, int oC,
                             const int8_t* w_psram, float w_scale,
                             int kH, int kW, int pad, int stride) {
  int oH = (iH + 2 * pad - (kH - 1) - 1) / stride + 1;
  int oW = (iW + 2 * pad - (kW - 1) - 1) / stride + 1;
  memset(out, 0, sizeof(float) * oC * oH * oW);

  for (int oc = 0; oc < oC; oc++) {
    float* out_c = out + oc * (oH * oW);
    const int8_t* w_oc = w_psram + oc * (iC * kH * kW);

    for (int ic = 0; ic < iC; ic++) {
      const float* in_c = in + ic * (iH * iW);
      const int8_t* w_ic = w_oc + ic * (kH * kW);

      for (int kh = 0; kh < kH; kh++) {
        for (int kw = 0; kw < kW; kw++) {
          int8_t qw = w_ic[kh * kW + kw];
          if (qw == 0) continue; // Sparse weight skip
          float w_val = qw * w_scale;

          for (int oh = 0; oh < oH; oh++) {
            int ih = oh * stride + kh - pad;
            if (ih < 0 || ih >= iH) continue;

            for (int ow = 0; ow < oW; ow++) {
              int iw = ow * stride + kw - pad;
              if (iw < 0 || iw >= iW) continue;

              out_c[oh * oW + ow] += in_c[ih * iW + iw] * w_val;
            }
          }
        }
      }
    }
  }
}

// ── Transpose 2D Convolution (Stride 2 Upsample) ─────────────────────────────
void SNNEngine::_convTranspose2d_simd(const float* in, int iC, int iH, int iW,
                                      float* out, int oC,
                                      const int8_t* w_psram, float w_scale) {
  int oH = iH * 2;
  int oW = iW * 2;
  memset(out, 0, sizeof(float) * oC * oH * oW);

  for (int ic = 0; ic < iC; ic++) {
    const float* in_c = in + ic * (iH * iW);
    for (int oc = 0; oc < oC; oc++) {
      float* out_c = out + oc * (oH * oW);
      const int8_t* w_ptr = w_psram + (ic * oC + oc) * 4;

      float w00 = w_ptr[0] * w_scale;
      float w01 = w_ptr[1] * w_scale;
      float w10 = w_ptr[2] * w_scale;
      float w11 = w_ptr[3] * w_scale;

      for (int ih = 0; ih < iH; ih++) {
        for (int iw = 0; iw < iW; iw++) {
          float val = in_c[ih * iW + iw];
          if (val == 0.0f) continue;

          int out_idx = (ih * 2) * oW + (iw * 2);
          out_c[out_idx]          += val * w00;
          out_c[out_idx + 1]      += val * w01;
          out_c[out_idx + oW]      += val * w10;
          out_c[out_idx + oW + 1]  += val * w11;
        }
      }
    }
  }
}

// ── Leaky Integrate-and-Fire (LIF) Step ──────────────────────────────────────
void SNNEngine::_lifStep(const float* current_in, float* mem, float* spikes_out,
                         int n, float beta, float vth) {
  for (int i = 0; i < n; i++) {
    // V_m(t) = beta * V_m(t-1) + I_in(t)
    float vm = beta * mem[i] + current_in[i];
    if (vm >= vth) {
      spikes_out[i] = 1.0f;
      vm -= vth; // Soft reset mechanism
    } else {
      spikes_out[i] = 0.0f;
    }
    mem[i] = vm;
  }
}

void SNNEngine::_softmax2ch(const float* in, float* out, size_t px) {
  for (size_t i = 0; i < px; i++) {
    float a = in[i];
    float b = in[px + i];
    float m = (a > b) ? a : b;
    float ea = expf(a - m);
    float eb = expf(b - m);
    float sum = ea + eb;
    out[i] = ea / sum;
    out[px + i] = eb / sum;
  }
}

// ── Coarse Sparse Difference Check ───────────────────────────────────────────
bool SNNEngine::_checkTileActivity(const float* t1, const float* t2, int total_px, float& mean_diff) {
  float sum_diff = 0.0f;
  for (int i = 0; i < total_px; i++) {
    sum_diff += fabsf(t2[i] - t1[i]);
  }
  mean_diff = sum_diff / (float)total_px;
  return (mean_diff >= SPARSE_DIFF_THRESH);
}

// ── Single Image Encoder Forward Pass ────────────────────────────────────────
void SNNEngine::_encodeSingle(const float* img, PSRAMTensor skips[4], float* bot_out) {
  const int H = SNN_PATCH_H;
  const int W = SNN_PATCH_W;

  static float* scratch = nullptr;
  if (!scratch) {
    scratch = (float*)heap_caps_malloc(BOTTLENECK_CH * H * W * sizeof(float), MALLOC_CAP_SPIRAM);
  }

  // Encoder Block 0: 1ch -> 8ch (128x128)
  _conv2d_simd(img, 1, H, W, skips[0].data, ENC_CH0, _w.enc0_conv1, _w.enc0_conv1_s);
  _batchNorm(skips[0].data, ENC_CH0, H * W, enc0_bn1_gamma, enc0_bn1_beta, enc0_bn1_mean, enc0_bn1_var);
  _relu(skips[0].data, ENC_CH0 * H * W);

  _conv2d_simd(skips[0].data, ENC_CH0, H, W, scratch, ENC_CH0, _w.enc0_conv2, _w.enc0_conv2_s);
  _batchNorm(scratch, ENC_CH0, H * W, enc0_bn2_gamma, enc0_bn2_beta, enc0_bn2_mean, enc0_bn2_var);
  _relu(scratch, ENC_CH0 * H * W);
  memcpy(skips[0].data, scratch, ENC_CH0 * H * W * sizeof(float)); // Keep pre-pool for skip
  _maxpool2x2(skips[0].data, scratch, ENC_CH0, H, W);              // 64x64

  // Encoder Block 1: 8ch -> 16ch (64x64)
  _conv2d_simd(scratch, ENC_CH0, H / 2, W / 2, skips[1].data, ENC_CH1, _w.enc1_conv1, _w.enc1_conv1_s);
  _batchNorm(skips[1].data, ENC_CH1, (H / 2) * (W / 2), enc1_bn1_gamma, enc1_bn1_beta, enc1_bn1_mean, enc1_bn1_var);
  _relu(skips[1].data, ENC_CH1 * (H / 2) * (W / 2));
  _maxpool2x2(skips[1].data, scratch, ENC_CH1, H / 2, W / 2);      // 32x32

  // Encoder Block 2: 16ch -> 32ch (32x32)
  _conv2d_simd(scratch, ENC_CH1, H / 4, W / 4, skips[2].data, ENC_CH2, _w.enc2_conv1, _w.enc2_conv1_s);
  _batchNorm(skips[2].data, ENC_CH2, (H / 4) * (W / 4), enc2_bn1_gamma, enc2_bn1_beta, enc2_bn1_mean, enc2_bn1_var);
  _relu(skips[2].data, ENC_CH2 * (H / 4) * (W / 4));
  _maxpool2x2(skips[2].data, scratch, ENC_CH2, H / 4, W / 4);      // 16x16

  // Encoder Block 3: 32ch -> 64ch (16x16)
  _conv2d_simd(scratch, ENC_CH2, H / 8, W / 8, skips[3].data, ENC_CH3, _w.enc3_conv1, _w.enc3_conv1_s);
  _batchNorm(skips[3].data, ENC_CH3, (H / 8) * (W / 8), enc3_bn1_gamma, enc3_bn1_beta, enc3_bn1_mean, enc3_bn1_var);
  _relu(skips[3].data, ENC_CH3 * (H / 8) * (W / 8));
  _maxpool2x2(skips[3].data, scratch, ENC_CH3, H / 8, W / 8);      // 8x8

  // Bottleneck: 64ch -> 128ch (8x8)
  _conv2d_simd(scratch, ENC_CH3, H / 16, W / 16, bot_out, BOTTLENECK_CH, _w.bot_conv1, _w.bot_conv1_s);
  _batchNorm(bot_out, BOTTLENECK_CH, (H / 16) * (W / 16), bot_bn1_gamma, bot_bn1_beta, bot_bn1_mean, bot_bn1_var);
  _relu(bot_out, BOTTLENECK_CH * (H / 16) * (W / 16));
}

// ── Full SNN Inference Pipeline ──────────────────────────────────────────────
SNNResult SNNEngine::infer(const float* img_t1, const float* img_t2, float threshold) {
  SNNResult res{};
  res.total_pixels = SNN_PATCH_H * SNN_PATCH_W;

  if (!_initialized) {
    Serial.println("[SNN] Engine not initialized!");
    return res;
  }

  uint32_t t_start = millis();
  const int H = SNN_PATCH_H;
  const int W = SNN_PATCH_W;
  const size_t total_px = (size_t)H * W;

  // 1. Sparse Frame Difference Optimization Check
  float mean_diff = 0.0f;
  if (SPARSE_DIFF_ENABLE && !_checkTileActivity(img_t1, img_t2, total_px, mean_diff)) {
    Serial.printf("[SNN] Sparse skip: macroblock mean diff (%.4f) < thresh (%.4f)\n",
                  mean_diff, SPARSE_DIFF_THRESH);
    memset(_changeMask, 0, total_px);
    res.changed_pixels = 0;
    res.change_percentage = 0.0f;
    res.inference_time_ms = millis() - t_start;
    res.skipped_sparse = true;
    res.valid = true;
    return res;
  }

  // 2. Encode T1 and T2 Images
  _encodeSingle(img_t1, _t1Skips, _bottleneck.data);

  PSRAMTensor bot_t2;
  bot_t2.allocate((size_t)BOTTLENECK_CH * (H / 16) * (W / 16));
  _encodeSingle(img_t2, _t2Skips, bot_t2.data);

  // 3. Compute Multi-Scale Feature Difference
  _absDiff(_bottleneck.data, bot_t2.data, _bottleneck.data, (size_t)BOTTLENECK_CH * (H / 16) * (W / 16));
  bot_t2.free_buf();

  for (int i = 0; i < 4; i++) {
    int cur_ch = (i == 0) ? ENC_CH0 : (i == 1) ? ENC_CH1 : (i == 2) ? ENC_CH2 : ENC_CH3;
    int cur_h  = H >> (i + 1);
    int cur_w  = W >> (i + 1);
    _absDiff(_t1Skips[i].data, _t2Skips[i].data, _diffSkips[i].data, (size_t)cur_ch * cur_h * cur_w);
  }

  // 4. Rate Coding: Sigmoid normalization for Poisson rate sampling
  _sigmoid(_bottleneck.data, (size_t)BOTTLENECK_CH * (H / 16) * (W / 16));

  // 5. Reset LIF Membrane Potentials & Spike Accumulator
  for (int i = 0; i < 8; i++) _lifStates[i].reset();
  _spikeAccumulator.zero();

  // Working buffers for decoder
  static float* dec_work = nullptr;
  if (!dec_work) {
    dec_work = (float*)heap_caps_malloc(BOTTLENECK_CH * H * W * sizeof(float), MALLOC_CAP_SPIRAM);
  }

  size_t bot_count = (size_t)BOTTLENECK_CH * (H / 16) * (W / 16);
  static float* poisson_spikes = nullptr;
  if (!poisson_spikes) {
    poisson_spikes = (float*)heap_caps_malloc(bot_count * sizeof(float), MALLOC_CAP_SPIRAM);
  }

  // 6. Temporal SNN Evaluation Loop (T = SNN_NUM_STEPS = 5)
  for (int t = 0; t < SNN_NUM_STEPS; t++) {
    // Generate Poisson spike trains from normalized difference
    for (size_t i = 0; i < bot_count; i++) {
      poisson_spikes[i] = ((esp_random() / (float)UINT32_MAX) < _bottleneck.data[i]) ? 1.0f : 0.0f;
    }

    // Decoder Block 0: 128ch -> 64ch (16x16)
    _convTranspose2d_simd(poisson_spikes, BOTTLENECK_CH, H / 16, W / 16, dec_work, ENC_CH3, _w.dec0_up, _w.dec0_up_s);
    _lifStep(dec_work, _lifStates[0].v_mem.data, dec_work, ENC_CH3 * (H / 8) * (W / 8));

    // Decoder Block 1: 64ch -> 32ch (32x32)
    _convTranspose2d_simd(dec_work, ENC_CH3, H / 8, W / 8, dec_work, ENC_CH2, _w.dec1_up, _w.dec1_up_s);
    _lifStep(dec_work, _lifStates[2].v_mem.data, dec_work, ENC_CH2 * (H / 4) * (W / 4));

    // Decoder Block 2: 32ch -> 16ch (64x64)
    _convTranspose2d_simd(dec_work, ENC_CH2, H / 4, W / 4, dec_work, ENC_CH1, _w.dec2_up, _w.dec2_up_s);
    _lifStep(dec_work, _lifStates[4].v_mem.data, dec_work, ENC_CH1 * (H / 2) * (W / 2));

    // Decoder Block 3: 16ch -> 8ch (128x128)
    _convTranspose2d_simd(dec_work, ENC_CH1, H / 2, W / 2, dec_work, ENC_CH0, _w.dec3_up, _w.dec3_up_s);
    _lifStep(dec_work, _lifStates[6].v_mem.data, dec_work, ENC_CH0 * H * W);

    // Output Conv: 8ch -> 2 classes (128x128)
    static float* out_classes = nullptr;
    if (!out_classes) {
      out_classes = (float*)heap_caps_malloc(2 * total_px * sizeof(float), MALLOC_CAP_SPIRAM);
    }
    _conv2d_simd(dec_work, ENC_CH0, H, W, out_classes, 2, _w.out_conv, _w.out_conv_s, 1, 1, 0, 1);

    // Accumulate output spikes over time
    for (size_t i = 0; i < 2 * total_px; i++) {
      _spikeAccumulator.data[i] += out_classes[i];
    }
  }

  // 7. Spike Firing Rate Averaging & Softmax Classification
  for (size_t i = 0; i < 2 * total_px; i++) {
    _spikeAccumulator.data[i] /= (float)SNN_NUM_STEPS;
  }

  static float* probs = nullptr;
  if (!probs) {
    probs = (float*)heap_caps_malloc(2 * total_px * sizeof(float), MALLOC_CAP_SPIRAM);
  }
  _softmax2ch(_spikeAccumulator.data, probs, total_px);

  int changed_count = 0;
  for (size_t i = 0; i < total_px; i++) {
    // Class 1 = "Changed"
    if (probs[total_px + i] > threshold) {
      _changeMask[i] = 1;
      changed_count++;
    } else {
      _changeMask[i] = 0;
    }
  }

  res.changed_pixels = changed_count;
  res.change_percentage = (100.0f * (float)changed_count) / (float)total_px;
  res.inference_time_ms = millis() - t_start;
  res.skipped_sparse = false;
  res.valid = true;

  Serial.printf("[SNN] Inference Complete: %d/%d px (%.2f%%) in %u ms\n",
                changed_count, (int)total_px, res.change_percentage, res.inference_time_ms);
  return res;
}
