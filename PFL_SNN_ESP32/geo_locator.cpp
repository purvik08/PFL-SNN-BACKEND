// geo_locator.cpp — Automated IP-Based Geolocation Engine Implementation
#include "geo_locator.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

bool fetchDeviceLocation(DeviceGeoTag& out) {
  out.resolved = false;
  strlcpy(out.city, "Surat", sizeof(out.city));
  strlcpy(out.country, "India", sizeof(out.country));
  out.lat = 21.1702f;
  out.lon = 72.8311f;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[GEO] Wi-Fi not connected. Using default geo-tag.");
    return false;
  }

  HTTPClient http;
  http.begin(GEO_API_URL);
  http.setTimeout(GEO_TIMEOUT_MS);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[GEO] HTTP request failed with code: %d\n", httpCode);
    http.end();
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();

  if (err) {
    Serial.printf("[GEO] JSON deserialization failed: %s\n", err.c_str());
    return false;
  }

  if (doc["status"] == "success" || doc.containsKey("city")) {
    strlcpy(out.city,    doc["city"]    | "Surat", sizeof(out.city));
    strlcpy(out.country, doc["country"] | "India", sizeof(out.country));
    out.lat = doc["lat"] | 21.1702f;
    out.lon = doc["lon"] | 72.8311f;
    out.resolved = true;

    Serial.printf("[GEO] Geolocation resolved: %s, %s (%.4f, %.4f)\n",
                  out.city, out.country, out.lat, out.lon);
    return true;
  }

  return false;
}
