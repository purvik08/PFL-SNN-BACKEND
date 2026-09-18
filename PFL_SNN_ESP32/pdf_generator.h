// pdf_generator.h — Zero-Dependency Embedded PDF 1.4 Binary Stream Generator
#pragma once
#include <Arduino.h>
#include <SD.h>
#include "config.h"

struct GeoCoordinates {
  char  city[48];
  char  country[32];
  float lat;
  float lon;
};

struct PDFReportMetadata {
  char           filepath[64];
  int            report_id;
  char           timestamp_str[36];
  GeoCoordinates geo;
  int            changed_pixels;
  int            total_pixels;
  float          change_percentage;
  float          threshold;
  uint32_t       inference_ms;
  bool           sparse_skipped;
};

class PDFGenerator {
public:
  PDFGenerator();
  bool generate(const PDFReportMetadata& meta, const uint8_t* mask, int w, int h);

private:
  File _file;
  long _objOffsets[24];
  int  _objIndex;

  long _currentOffset();
  void _writeString(const char* str);
  void _writeFormatted(const char* fmt, ...);
  int  _startObject();
  void _finishObject();

  void _writeHeader();
  void _writeCatalog();
  void _writePages();
  void _writePage();
  void _writeContentStream(const PDFReportMetadata& meta, const uint8_t* mask, int w, int h);
  void _writeFont();
  void _writeXRefTable();
  void _writeTrailer();
  void _writeInlineMaskImage(const uint8_t* mask, int w, int h);
};

extern PDFGenerator pdfGenerator;
