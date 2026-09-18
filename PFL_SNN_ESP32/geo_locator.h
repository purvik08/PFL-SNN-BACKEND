// geo_locator.h — Automated IP-Based Geolocation Engine
#pragma once
#include "config.h"

struct DeviceGeoTag {
  char  city[48];
  char  country[32];
  float lat;
  float lon;
  bool  resolved;
};

bool fetchDeviceLocation(DeviceGeoTag& out);
