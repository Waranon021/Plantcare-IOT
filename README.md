# PlantCare IoT

**Soil wetness and thermal exposure monitoring for a potted plant.** PlantCare combines an ESP32, an analog soil sensor, an AMG8833 8 × 8 infrared array, and an LCD1602. It displays readings at the pot and on the ESP32's local web page, then sends sensor snapshots over HTTPS to Node-RED. InfluxDB stores the readings and Grafana presents current values, trends, experimental status labels, and historical data.

This is a monitoring prototype for one *Portulaca* pot. The owner decides whether to water or move the plant. There is no automatic irrigation, camera-based bloom detector, or trained machine-learning model in this source set.

- [Final report](report/PlantCare-Final-Report.pdf)
- [Offline evidence viewer](backup/index.html) — download the repository and open this file in a browser.
- [Offline evidence instructions](backup/README.md)
- [Original Grafana dashboard](grafana/dashboard.json)
- [Dashboard Recovery](<grafana/Dashboard Recovery.json>)

Presentation slides and speaking/demo scripts are submitted separately and are excluded from this repository.

## How the project works

1. The ESP32 samples the soil sensor on GPIO34 and the AMG8833 on the GPIO21/22 I²C bus. The LCD uses the same bus.
2. It averages 20 soil ADC readings, rejects unusable readings, and applies an exponential moving average with alpha 0.25. It converts the filtered value into a clipped 0–100 relative wetness index.
3. The LCD, Serial Monitor, and local web routes use the cached sensor snapshot. The web view includes the thermal image and interpolates its display from 64 measured pixels; interpolation does not add sensor resolution.
4. The HTTPS uploader validates the server using its configured root CA and sends JSON to `POST /plantcare/ingest`, with a shared key in `x-plantcare-key`.
5. Node-RED validates the key, identifiers, and sensor data. Its three HTTPS outputs are the HTTP response, a soil write, and a thermal write. Thermal processing produces minimum/maximum, left/right half means, their absolute difference, and pixel fields `p00`–`p77`.
6. InfluxDB stores the `plantcare_soil` and `plantcare_thermal` measurements. Grafana queries the `plantcare` bucket, filtering for `data_source=real`.

The separate MQTT subscriber listens to `plantcare/ait/plant01/esp32_01/+` and feeds a debug node. It documents the MQTT experiment; the ESP32 sketch uses HTTPS for its live uplink. The supplied flow also has simulated validation and manual write branches, which use `data_source=simulated`.

## Current source versus the reported trial

The supplied local source is newer than the configuration described in the final report. The source is retained with the following differences made explicit:

| Setting | Supplied current `.ino` | Final report's trial description |
| --- | --- | --- |
| Sensor sampling | 2,000 ms | Approximately 2 seconds |
| Upload attempt interval | **2,000 ms** | Approximately 60 seconds |
| Drier/wetter soil ADC references | **2492 / 2152** | **2614 / 2274** |
| EMA alpha | 0.25 | 0.25 |

The upload interval schedules attempts; network requests can block, so it is not a measured delivery rate. The old 60-second configuration's nominal 30-to-1 reduction does not describe the current 2-second uploader. No firmware timing or calibration was silently changed to make it match the report.

Current normalization is `clip(100 * (2492 - filtered_ADC) / (2492 - 2152), 0, 100)`. This is a relative index for this setup, not volumetric soil water content. Historical screenshots and report results retain the interpretation and calibration of their original trial.

The supplied PDF is kept unchanged. Its LaTeX source was supplied separately and was not rebuilt here; the existing PDF may not exactly reflect every edit in that source.

## Hardware and software

| Component | Role |
| --- | --- |
| ESP32 development board | Sensor acquisition, filtering, local web server, LCD updates, HTTPS upload |
| Analog soil moisture sensor | Soil response at one position in the pot; signal on GPIO34 (ADC1) |
| AMG8833 | 64 infrared pixel temperatures over I²C |
| LCD1602 with I²C interface | Local readings without depending on the server |
| Arduino IDE with ESP32 board support | Build and upload the sketch |
| Adafruit AMG88xx and its dependencies | AMG8833 interface |
| LiquidCrystal_I2C | LCD interface; use a library compatible with the calls in the sketch |
| Node-RED + `node-red-contrib-influxdb` 0.7.0 | Ingestion and database writes |
| InfluxDB 2 with Flux | Time-series storage and dashboard queries |
| Grafana | Live and historical visualization |

The report describes a lab-hosted Proxmox LXC platform. This repository does not include a Proxmox image, Docker Compose deployment, a live database, or a verified Cron export script.

## Set up the ESP32

1. Install ESP32 board support and the libraries above. Select the actual connected board and serial port.
2. Open `firmware/plantcare_main/plantcare_main.ino` in Arduino IDE.
3. Copy `credentials.example.h` to `credentials.h` in the same sketch folder. Fill in the local Wi-Fi name/password, your HTTPS ingest URL, and a randomly generated key of at least 32 characters. Match that key to `PLANTCARE_INGEST_KEY` on Node-RED. Never commit `credentials.h`.
4. Check wiring, I²C addresses, LCD compatibility, and the soil reference values for the actual setup. Do not assume one pot's calibration is transferable to another.
5. Compile and upload. Open Serial Monitor at **115200 baud**. When Wi-Fi connects, use the printed local IP address from a device on a reachable LAN.

| Local route | Purpose |
| --- | --- |
| `/` | ESP32 browser dashboard |
| `/soil` | Cached soil JSON |
| `/thermal` | Cached thermal JSON; returns 503 when a frame is unavailable |
| `/status` | Device/sensor status JSON |

The LCD and local sensing can continue during an uplink interruption. The web page still needs a reachable local network. Local routes use HTTP without route-level authentication; keep the board on a trusted LAN and do not expose these routes to the internet. Missed uploads are not replayed later.

`plantcare_cloud.example.h` preserves a sanitized copy of the old optional helper. The main sketch already contains its own HTTPS uploader and does **not** include this helper. Do not include both without reconciling their duplicated definitions. The helper retains its own 60-second setting and is not the current timing authority.

## Set up Node-RED and InfluxDB

1. Prepare InfluxDB 2, an organization, and a bucket named `plantcare`. The exported write nodes retain the trial organization name `plantcare-group12`; update every write node if your organization differs.
2. Give Node-RED an InfluxDB token with only the bucket permissions it needs. Enter the token in the InfluxDB config node's credentials editor; do not store it in the flow JSON.
3. Install `node-red-contrib-influxdb` **0.7.0** through Manage Palette, then import `node-red/flows.json`.
4. Set the following environment variables on the **Node-RED process/service**, then restart it. `node-red/.env.example` documents the names; copying that file alone does not make Node-RED load it.
5. Configure the InfluxDB write nodes and deploy. Put the ingest endpoint behind correctly configured HTTPS. The firmware's root CA must validate your server's certificate chain.

| Environment variable | Meaning |
| --- | --- |
| `PLANTCARE_INGEST_KEY` | Private shared key, at least 32 characters; must match the ESP32 |
| `PLANTCARE_INFLUX_HOST` | Internal InfluxDB hostname |
| `PLANTCARE_INFLUX_URL` | InfluxDB base URL, including scheme and port |
| `PLANTCARE_INFLUX_HEALTH_URL` | InfluxDB health endpoint URL |
| `PLANTCARE_MQTT_HOST` | Broker hostname for the optional MQTT debug branch |

The exported tab no longer contains a stored ingest key. Debug nodes are disabled in the distributed flow. Enable only the specific debug node needed during a private test and avoid logging whole request objects or headers. Configure service credentials in Node-RED; do not commit `flows_cred.json`.

Use TLS for InfluxDB across an untrusted network, or restrict its internal HTTP endpoint to the trusted lab network. The InfluxDB token is separate from the ESP32 ingest key.

### HTTPS contract

Send `Content-Type: application/json` and the private `x-plantcare-key` header. A soil-only example body is:

```json
{
  "plant_id": "plant01",
  "device_id": "esp32_01",
  "data_source": "real",
  "soil": {
    "sensor_ok": true,
    "raw": 2468,
    "filtered_raw": 2466,
    "relative_wetness_pct": 7.65
  }
}
```

The thermal section, when present, is `{"pixels": [...]}` with exactly 64 finite temperatures between -40 and 125 °C. At least one valid sensor section is required. Metadata is restricted to `plant01`, `esp32_01`, and `real` or `simulated` by the supplied function.

| Response | Meaning |
| --- | --- |
| 202 | Validated and accepted for downstream processing; **not proof of a durable database write** |
| 401 | Missing, invalid, or unconfigured key |
| 400 | Invalid JSON, payload, metadata, sensor section, or no sensor data |

Verify persistence separately with an InfluxDB query after a controlled upload.

### Stored measurements

| Measurement | Fields |
| --- | --- |
| `plantcare_soil` | `soil_adc_raw`, `soil_adc_filtered`, `relative_wetness_pct` |
| `plantcare_thermal` | `t_min_c`, `t_max_c`, `t_left_c`, `t_right_c`, `delta_t_c`, `p00`–`p77` |
| Common tags | `plant_id`, `device_id`, `data_source` |

Left/right refer to the two halves of the sensor array. A pixel can contain leaves, pot, wall, or background; these readings are not automatically leaf temperatures or a diagnosis of plant stress.

## Grafana and recovery

| File or mode | What it provides | Needs InfluxDB? |
| --- | --- | --- |
| `grafana/dashboard.json` | Original repository dashboard, preserved byte-for-byte | Yes |
| `grafana/Dashboard Recovery.json` | Supplied recovered dashboard, renamed **Dashboard Recovery**, with a separate identifier | Yes |
| `backup/index.html` and `backup/assets/` | Offline viewer of the supplied historical screenshots | **No** |

Both JSON files use Grafana's `dashboard.grafana.app/v2` export format. Import them into a Grafana instance that supports that format. The recovered export records visualization version 13.2.2. Compatibility with another Grafana version must be checked on that instance.

Configure a Flux data source for the correct InfluxDB organization and bucket. Rebind the dashboard's saved data-source references after import; a selector alone may not replace a panel's fixed reference. Use a different dashboard identity for Recovery so the original is not overwritten. Import behavior was checked structurally, not against a live Grafana service.

The original dashboard includes current values, ADC/wetness/thermal trends, trial status, telemetry gaps, a thermal matrix, and history. Current and status panels can use short lookback windows even when a wider history range is selected. The Recovery default is a 15-day lookback with automatic refresh off; it still queries the database and does not embed old readings. If data is outside the selected window, choose the actual trial dates.

The trial's soil bands are below 17%, 17–20%, 20–70%, and at least 70%. Thermal trial rules use a maximum of 35 °C and a left/right difference of 2 °C. These are experimental labels, not validated plant-health advice. A connectivity label reflects telemetry freshness, not a direct Wi-Fi measurement. An empty result and a failed data-source query need different troubleshooting.

### Use the offline backup

Download/clone the repository, then double-click `backup/index.html`. Keep its `assets` folder alongside it. The page works without a local server and uses no external scripts, fonts, API, or internet requests. Click an image for its original resolution.

The images preserve the report's dashboard overview and the team's local recovered-dashboard history. The local view displays 3-17 November 2026; the 29 October start comes from the report's earlier audit. Its explanation records the shared Grafana issue, early interruptions, improved later consistency, and calibration adjustments from around 10 November. **All values and freshness labels are historical, not live.** This is a submission fallback, not a time-series database restore. No raw CSV was supplied in the local project folder; no missing records have been invented.

## Report results and evidence limits

The final report records four before/after watering observations with lower ADC values after watering. Its separate saved Explore CSV audit reports 5,860 minute-level rows, with complete soil/thermal fields in 5,844 rows, and long data gaps. That CSV itself is not included in the supplied source folder, so these counts are attributed to the report rather than presented as a new audit of raw data.

The project does not establish measured water savings, plant stress, service uptime, energy efficiency, or a measured cloud-only performance comparison. The reported daily CSV schedule and 180-day retention need live configuration evidence. The old manuscript's nine offline tests describe the earlier verification; its old test file is not retained in this source replacement.

## File guide

Paths below are relative to the repository root. Template fonts are kept because the supplied IEEE Access source depends on them. They are not application code.

<!-- FILE_INVENTORY -->

| File | Purpose |
| --- | --- |
| `.gitattributes` | Text line-ending and binary file handling. |
| `.gitignore` | Excludes private credentials, runtime outputs, slides, and presentation scripts. |
| `backup/assets/grafana-local-history.png` | Local recovered dashboard for 3-17 November 2026, cropped only to remove the browser URL; supporting evidence with calibration caveats. Offline viewer asset. |
| `backup/assets/grafana-overview.png` | Saved full dashboard with trends, status, thermal matrix, and history. Offline viewer asset. |
| `backup/index.html` | Standalone offline evidence viewer; no live requests or generated sensor values. |
| `backup/README.md` | Offline use instructions and evidence limits. |
| `firmware/plantcare_main/credentials.example.h` | Safe template for the untracked credentials.h used by the sketch. |
| `firmware/plantcare_main/plantcare_cloud.example.h` | Sanitized legacy cloud helper; not included by the active sketch. |
| `firmware/plantcare_main/plantcare_main.ino` | Active ESP32 sketch: sensing, soil processing, local display/web routes, and HTTPS upload. |
| `grafana/Dashboard Recovery.json` | Recovered Grafana export with independent name/title; still queries InfluxDB. |
| `grafana/dashboard.json` | Original repository Grafana export, preserved byte-for-byte. |
| `node-red/.env.example` | Documents environment names; contains no real key and is not automatically loaded by Node-RED. |
| `node-red/flows.json` | Imported Node-RED flow: HTTPS ingest, InfluxDB outputs, MQTT debug, simulated/manual checks; private values removed. |
| `README.md` | Project overview, setup, source/report differences, recovery instructions, and file inventory. |
| `report/bullet.png` | Bullet graphic used by the document template. |
| `report/figures/architecture.pdf` | Four-layer architecture diagram, including the separate MQTT debug branch. Report figure. |
| `report/figures/grafana-overview.png` | Saved full dashboard with trends, status, thermal matrix, and history. Report figure. |
| `report/figures/grafana-two-days.png` | Saved two-day soil/thermal availability view. Report figure. |
| `report/figures/grafana-watering.png` | Saved wetness/ADC plots around the reported watering events. Report figure. |
| `report/figures/grafana.png` | Additional supplied Grafana screenshot retained with the report assets. Report figure. |
| `report/figures/hardware.jpg` | Photographs of the ESP32/LCD, hanging pot, and monitored plant. Report figure. |
| `report/figures/node-red-ingest.png` | HTTPS ingest function and its HTTP/soil/thermal outputs. Report figure. |
| `report/ieeeaccess.cls` | IEEE Access document class used by the main manuscript. |
| `report/IEEEtran.bst` | IEEE bibliography formatting style supplied with the template. |
| `report/IEEEtran.cls` | IEEE document class supplied with the template. |
| `report/logo.png` | IEEE Access template logo asset. |
| `report/notaglinelogo.png` | Alternate IEEE Access template logo asset. |
| `report/PlantCare-Final-Report.pdf` | Supplied final manuscript PDF, unchanged. |
| `report/PlantCare-Final-Report.tex` | Main IEEE Access manuscript source; includes results.tex and references.tex. |
| `report/README.md` | Report build handoff and notes about source/PDF differences. |
| `report/references.tex` | Manuscript bibliography entries. |
| `report/results.tex` | Experimental observations, CSV audit described in the report, dashboard rules, limits, and conclusion. |
| `report/t1-formata-bold.pfb` | Embedded Type 1 font outline (t1-formata-bold) for the supplied manuscript template. |
| `report/t1-formata-bold.tfm` | TeX font metrics (t1-formata-bold) matching the supplied template font. |
| `report/t1-formata-bolditalic.pfb` | Embedded Type 1 font outline (t1-formata-bolditalic) for the supplied manuscript template. |
| `report/t1-formata-bolditalic.tfm` | TeX font metrics (t1-formata-bolditalic) matching the supplied template font. |
| `report/t1-formata-condmedium.pfb` | Embedded Type 1 font outline (t1-formata-condmedium) for the supplied manuscript template. |
| `report/t1-formata-condmedium.tfm` | TeX font metrics (t1-formata-condmedium) matching the supplied template font. |
| `report/t1-formata-condmediumital.pfb` | Embedded Type 1 font outline (t1-formata-condmediumital) for the supplied manuscript template. |
| `report/t1-formata-condmediumital.tfm` | TeX font metrics (t1-formata-condmediumital) matching the supplied template font. |
| `report/t1-formata-italic.pfb` | Embedded Type 1 font outline (t1-formata-italic) for the supplied manuscript template. |
| `report/t1-formata-italic.tfm` | TeX font metrics (t1-formata-italic) matching the supplied template font. |
| `report/t1-formata-light.pfb` | Embedded Type 1 font outline (t1-formata-light) for the supplied manuscript template. |
| `report/t1-formata-light.tfm` | TeX font metrics (t1-formata-light) matching the supplied template font. |
| `report/t1-formata-lightitalic.pfb` | Embedded Type 1 font outline (t1-formata-lightitalic) for the supplied manuscript template. |
| `report/t1-formata-lightitalic.tfm` | TeX font metrics (t1-formata-lightitalic) matching the supplied template font. |
| `report/t1-formata-medium.pfb` | Embedded Type 1 font outline (t1-formata-medium) for the supplied manuscript template. |
| `report/t1-formata-medium.tfm` | TeX font metrics (t1-formata-medium) matching the supplied template font. |
| `report/t1-formata-mediumitalic.pfb` | Embedded Type 1 font outline (t1-formata-mediumitalic) for the supplied manuscript template. |
| `report/t1-formata-mediumitalic.tfm` | TeX font metrics (t1-formata-mediumitalic) matching the supplied template font. |
| `report/t1-formata-regsymb.pfb` | Embedded Type 1 font outline (t1-formata-regsymb) for the supplied manuscript template. |
| `report/t1-formata-regsymb.tfm` | TeX font metrics (t1-formata-regsymb) matching the supplied template font. |
| `report/t1-formata-regular.pfb` | Embedded Type 1 font outline (t1-formata-regular) for the supplied manuscript template. |
| `report/t1-formata-regular.tfm` | TeX font metrics (t1-formata-regular) matching the supplied template font. |
| `report/t1-formata.map` | Font mapping (t1-formata) between TeX font names and font files. |
| `report/t1-giovannistd-bookitalic.pfb` | Embedded Type 1 font outline (t1-giovannistd-bookitalic) for the supplied manuscript template. |
| `report/t1-giovannistd-bookitalic.tfm` | TeX font metrics (t1-giovannistd-bookitalic) matching the supplied template font. |
| `report/t1-giovannistd.map` | Font mapping (t1-giovannistd) between TeX font names and font files. |
| `report/t1-helvetica.map` | Font mapping (t1-helvetica) between TeX font names and font files. |
| `report/t1-times-bold.pfb` | Embedded Type 1 font outline (t1-times-bold) for the supplied manuscript template. |
| `report/t1-times-bold.tfm` | TeX font metrics (t1-times-bold) matching the supplied template font. |
| `report/t1-times-bolditalic.pfb` | Embedded Type 1 font outline (t1-times-bolditalic) for the supplied manuscript template. |
| `report/t1-times-bolditalic.tfm` | TeX font metrics (t1-times-bolditalic) matching the supplied template font. |
| `report/t1-times-italic.pfb` | Embedded Type 1 font outline (t1-times-italic) for the supplied manuscript template. |
| `report/t1-times-italic.tfm` | TeX font metrics (t1-times-italic) matching the supplied template font. |
| `report/t1-times.map` | Font mapping (t1-times) between TeX font names and font files. |
| `report/t1-times.pfb` | Embedded Type 1 font outline (t1-times) for the supplied manuscript template. |
| `report/t1-times.tfm` | TeX font metrics (t1-times) matching the supplied template font. |
| `report/t1formata.fd` | LaTeX font-family definition (t1formata) for the manuscript template. |
| `report/t1giovannistd.fd` | LaTeX font-family definition (t1giovannistd) for the manuscript template. |
| `report/t1helvetica.fd` | LaTeX font-family definition (t1helvetica) for the manuscript template. |
| `report/t1times.fd` | LaTeX font-family definition (t1times) for the manuscript template. |
