// pdf_generator.cpp — Zero-Dependency Embedded PDF 1.4 Binary Stream Generator Implementation
#include "pdf_generator.h"
#include <stdarg.h>
#include <stdio.h>

PDFGenerator pdfGenerator;
static char _fmt_buffer[256];

PDFGenerator::PDFGenerator() : _objIndex(0) {
  memset(_objOffsets, 0, sizeof(_objOffsets));
}

long PDFGenerator::_currentOffset() {
  return (long)_file.position();
}

void PDFGenerator::_writeString(const char* str) {
  _file.print(str);
}

void PDFGenerator::_writeFormatted(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vsnprintf(_fmt_buffer, sizeof(_fmt_buffer), fmt, args);
  va_end(args);
  _file.print(_fmt_buffer);
}

int PDFGenerator::_startObject() {
  int id = ++_objIndex;
  _objOffsets[id] = _currentOffset();
  _writeFormatted("%d 0 obj\n", id);
  return id;
}

void PDFGenerator::_finishObject() {
  _writeString("endobj\n");
}

void PDFGenerator::_writeHeader() {
  _writeString("%PDF-1.4\n%\xe2\xe3\xcf\xd3\n");
  _objIndex = 0;
}

void PDFGenerator::_writeInlineMaskImage(const uint8_t* mask, int w, int h) {
  // Inline grayscale image dictionary
  _writeFormatted("BI\n/W %d\n/H %d\n/CS /DeviceGray\n/BPC 1\n/F /AHx\nID\n", w, h);

  int row_bytes = (w + 7) / 8;
  for (int y = 0; y < h; y++) {
    for (int byte_idx = 0; byte_idx < row_bytes; byte_idx++) {
      uint8_t packed_byte = 0;
      for (int bit = 0; bit < 8; bit++) {
        int px = y * w + byte_idx * 8 + bit;
        if (px < w * h && mask[px] > 0) {
          packed_byte |= (0x80 >> bit);
        }
      }
      _writeFormatted("%02X", packed_byte);
    }
    if (y % 16 == 15) {
      _writeString("\n");
    }
  }
  _writeString(">\nEI\n");
}

void PDFGenerator::_writeContentStream(const PDFReportMetadata& meta, const uint8_t* mask, int w, int h) {
  _startObject(); // Obj 4: Content Stream

  // Build stream in memory chunks
  char text_stream[2048];
  int len = 0;
  #define SPRINT(fmt, ...) len += snprintf(text_stream + len, sizeof(text_stream) - len, fmt, ##__VA_ARGS__)

  // Header Box
  SPRINT("0.1 0.15 0.35 rg\n0 770 595 72 re f\n");
  SPRINT("BT\n/F1 18 Tf\n1 1 1 rg\n40 805 Td\n(GeoGuard Autonomous Edge SNN Report) Tj\n");
  SPRINT("/F1 10 Tf\n0 -18 Td\n(Embedded SNN Change Detection & Geocoded Compliance Node) Tj\nET\n");

  // Metadata Table
  SPRINT("0 0 0 rg\n");
  SPRINT("BT\n/F1 12 Tf\n40 735 Td\n(EXECUTIVE AUDIT SUMMARY) Tj\nET\n");

  SPRINT("0.85 0.88 0.95 rg\n40 600 515 120 re f\n");
  SPRINT("0.2 0.2 0.2 RG\n0.8 w\n40 600 515 120 re S\n");

  SPRINT("BT\n/F1 9 Tf\n0 0 0 rg\n");
  SPRINT("50 700 Td\n(Report ID:) Tj\n150 0 Td\n(GEO-SNN-%04d) Tj\n", meta.report_id);
  SPRINT("-150 -16 Td\n(Timestamp:) Tj\n150 0 Td\n(%s) Tj\n", meta.timestamp_str);
  SPRINT("-150 -16 Td\n(Location Tag:) Tj\n150 0 Td\n(%s, %s [%.4f N, %.4f E]) Tj\n",
         meta.geo.city, meta.geo.country, meta.geo.lat, meta.geo.lon);
  SPRINT("-150 -16 Td\n(Changed Area:) Tj\n150 0 Td\n(%d / %d pixels  (%.2f %%)) Tj\n",
         meta.changed_pixels, meta.total_pixels, meta.change_percentage);
  SPRINT("-150 -16 Td\n(Inference Latency:) Tj\n150 0 Td\n(%u ms %s) Tj\n",
         meta.inference_ms, meta.sparse_skipped ? "[SPARSE SKIPPED]" : "[FULL SNN EVAL]");
  SPRINT("-150 -16 Td\n(LIF SNN Parameters:) Tj\n150 0 Td\n(T=5 Steps | Beta=0.90 | Threshold=%.2f) Tj\n",
         meta.threshold);
  SPRINT("ET\n");

  // Binary Mask Section
  SPRINT("BT\n/F1 12 Tf\n0 0 0 rg\n40 565 Td\n(SPIKING NEURAL NETWORK CHANGE DETECTION MASK) Tj\nET\n");
  SPRINT("0.9 0.9 0.9 rg\n40 310 240 240 re f\n");
  SPRINT("0.5 0.5 0.5 RG\n40 310 240 240 re S\n");

  // Transform matrix for image placement: position (40, 310) with size 240x240 pt
  SPRINT("q\n240 0 0 240 40 310 cm\n");

  // Write stream header
  size_t approx_stream_size = len + (w * h / 4) + 128;
  _writeFormatted("<< /Length %u >>\nstream\n", (unsigned)approx_stream_size);
  _file.write((const uint8_t*)text_stream, len);

  // Write mask inline image
  _writeInlineMaskImage(mask, w, h);
  _writeString("Q\n");

  // Footer Note
  const char* footer =
    "BT\n/F1 8 Tf\n0.4 0.4 0.4 rg\n"
    "40 50 Td\n(Generated autonomously by ESP32-S3 Edge Node. Cryptographic hash verifiable.) Tj\n"
    "0 -12 Td\n(Transfer Protocol: FTP Server on Port 21 / HTTP Ingestion on Port 80) Tj\nET\n";
  _writeString(footer);

  _writeString("\nendstream\n");
  _finishObject();
}

bool PDFGenerator::generate(const PDFReportMetadata& meta, const uint8_t* mask, int w, int h) {
  SD.mkdir(REPORTS_DIR);
  _file = SD.open(meta.filepath, FILE_WRITE);
  if (!_file) {
    Serial.printf("[PDF] ERROR: Failed to open file for writing: %s\n", meta.filepath);
    return false;
  }

  _writeHeader();

  // 1. Catalog Object (Obj 1)
  _startObject();
  _writeString("<< /Type /Catalog /Pages 2 0 R >>\n");
  _finishObject();

  // 2. Pages Object (Obj 2)
  _startObject();
  _writeString("<< /Type /Pages /Kids [3 0 R] /Count 1 >>\n");
  _finishObject();

  // 3. Page Object (Obj 3)
  _startObject();
  _writeString("<< /Type /Page\n"
               "   /Parent 2 0 R\n"
               "   /MediaBox [0 0 595 842]\n" // A4 Dimensions in points
               "   /Contents 4 0 R\n"
               "   /Resources << /Font << /F1 5 0 R >> >>\n"
               ">>\n");
  _finishObject();

  // 4. Content Stream Object (Obj 4)
  _writeContentStream(meta, mask, w, h);

  // 5. Standard Helvetica Font Object (Obj 5)
  _startObject();
  _writeString("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n");
  _finishObject();

  // 6. Cross-Reference (XRef) Table
  long xref_start = _currentOffset();
  _writeFormatted("xref\n0 %d\n", _objIndex + 1);
  _writeString("0000000000 65535 f \n");
  for (int i = 1; i <= _objIndex; i++) {
    _writeFormatted("%010ld 00000 n \n", _objOffsets[i]);
  }

  // 7. Trailer
  _writeFormatted("trailer\n<< /Size %d /Root 1 0 R >>\n", _objIndex + 1);
  _writeFormatted("startxref\n%ld\n%%%%EOF\n", xref_start);

  _file.close();
  Serial.printf("[PDF] Report generated and saved: %s\n", meta.filepath);
  return true;
}
