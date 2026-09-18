# API & Web Interface Reference — GeoGuard ESP32-S3

Reference for HTTP endpoints, FTP services, input formats, and report generation.

---

## 1. HTTP Web Server Endpoints (Port 80)

### `GET /`
- **Description**: Serves the main monitoring dashboard UI from LittleFS (`/index.html`).
- **Content-Type**: `text/html`

---

### `POST /ingest`
- **Description**: Ingests two temporal image patches ($T_1$ and $T_2$) for on-chip SNN change detection.
- **Content-Type**: `multipart/form-data` or raw binary payload
- **Parameters**:
  - `t1`: Image patch captured at time $T_1$ (128x128 grayscale / normalized float)
  - `t2`: Image patch captured at time $T_2$ (128x128 grayscale / normalized float)
  - `threshold` *(optional)*: Override change classification decision threshold (default: `0.50`)
- **Response**: `application/json`
```json
{
  "status": "success",
  "report_id": 1,
  "changed_pixels": 2341,
  "total_pixels": 16384,
  "change_percentage": 14.29,
  "inference_time_ms": 118,
  "sparse_skipped": false,
  "report_url": "http://192.168.1.105/report/latest"
}
```

---

### `GET /report/latest`
- **Description**: Streams the most recently generated geo-tagged PDF report directly from the MicroSD card.
- **Content-Type**: `application/pdf`
- **Response Header**: `Content-Disposition: inline; filename="report_latest.pdf"`

---

### `GET /status`
- **Description**: System health, FreeRTOS metrics, PSRAM utilization, and network state.
- **Content-Type**: `application/json`
```json
{
  "device": "GeoGuard ESP32-S3",
  "firmware": "1.0.0",
  "ip": "192.168.1.105",
  "location": {
    "city": "Jaipur",
    "country": "India",
    "lat": 26.9124,
    "lon": 75.7873
  },
  "memory": {
    "psram_total_mb": 8,
    "psram_free_kb": 6144,
    "heap_free_kb": 182
  },
  "reports_generated": 14
}
```

---

## 2. FTP Server Access (Port 21)

Direct file access to SD card storage for automated report fetching:

- **Host**: `<ESP32_IP_ADDRESS>`
- **Port**: `21`
- **Username**: `geoguard`
- **Password**: `geoguard123`
- **Report Path**: `/reports/report_*.pdf`

---

## 3. Autonomous Geolocation Engine

Upon establishing a Wi-Fi connection, the device performs an automated lookup against:
```
http://ip-api.com/json/
```
The resulting latitude, longitude, city, and country are embedded into every subsequent PDF report header and rendered on the TFT display screen.
