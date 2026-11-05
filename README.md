# PlantCare IoT

PlantCare monitors relative soil wetness and surface-temperature variation around a potted Portulaca. An ESP32 samples a Soil Moisture Sensor and an AMG8833 8 × 8 infrared array. The LCD and local web view show current readings. The plant owner decides when to water or adjust shade.

## Repository layout

| Path | Purpose |
| --- | --- |
| firmware/plantcare_main/ | Arduino sketch and a credential template |
| node-red/flows.json | HTTPS ingest, validation, two InfluxDB outputs, and separate test branches |
| grafana/dashboard.json | Grafana V2 Resource dashboard with 11 panels, including the thermal matrix |
| tests/ingest.test.mjs | Offline tests of the ingest function, including all 64 pixel positions |
| docs/report/ | IEEE Access LaTeX manuscript, bibliography, template assets, figures, and compiled report |
| docs/PlantCare-IoT-Final-Presentation.pptx | Presentation with speaker notes and full Grafana view |
| docs/PlantCare-Presentation-Script.md | English speaker script with Thai timing and demo cues |

## Data flow

1. The ESP32 reads the Soil Moisture Sensor on ADC1 GPIO34 and the thermal sensor over I²C (SDA GPIO21, SCL GPIO22). It averages 20 ADC readings, applies an exponential moving average with weight 0.25, and maps the filtered value to an experimental 0–100% relative index.
2. It updates local readings about every two seconds. It attempts an HTTPS POST to /plantcare/ingest about once a minute when Wi-Fi, clock, configuration, and at least one sensor are ready. The JSON carries plant ID, device ID, source tag, and available sensor sections.
3. Node-RED validates the x-plantcare-key header and payload. A valid request returns HTTP 202 and emits separate soil and thermal writes. Query the database to confirm that accepted telemetry persisted.
4. InfluxDB 2 stores plantcare_soil and plantcare_thermal in the plantcare bucket. For thermal data, Node-RED averages the left and right halves of the array and stores the absolute difference, frame minimum and maximum, and the 64 pixel fields p00–p77. The first digit identifies the row and the second identifies the column.
5. Grafana uses Flux to show current readings, freshness, status, history, and the 8 × 8 thermal matrix. The panels filter for data_source=real. The MQTT subscriber listens to the PlantCare test topic at QoS 0 and feeds a debug node. The ESP32 uses HTTPS for live readings. The separate manual write tests use simulated tags.

## Local setup

### Firmware

- Open firmware/plantcare_main/plantcare_main.ino in Arduino IDE with the ESP32 board package. Install Adafruit AMG88xx and a compatible LiquidCrystal_I2C library. Select the board and serial port for your ESP32.
- Copy credentials.example.h to credentials.h in the sketch folder. Set the Wi-Fi SSID and password, HTTPS endpoint, shared key of at least 32 characters, and trusted CA certificate for the endpoint. The template is intentionally nonfunctional until configured. Keep the filled file local. The sketch verifies server certificates and uses NTP for wall-clock time.
- Flash the sketch and check Serial Monitor. Check the local device page from a reachable LAN. The page provides /soil, /thermal and /status endpoints. Keep these plain HTTP endpoints on a trusted network.

### Node-RED and InfluxDB

- On your lab or test system, create an InfluxDB 2 bucket named plantcare and use a scoped write token for Node-RED. Install node-red-contrib-influxdb 0.7.0, the version recorded in the export.
- Import node-red/flows.json. Replace the example InfluxDB host and organization with your deployment values and set the token through the Node-RED credential field. Configure PLANTCARE_INGEST_KEY in the Node-RED service environment, or set it locally in the flow environment. It must match the ESP32 key. Expose the ingest path via a trusted HTTPS endpoint. The optional MQTT test broker starts with automatic connection disabled.
- Run the local test with Node.js by typing **node tests/ingest.test.mjs**. It exercises nine cases using synthetic payloads, including all 64 thermal pixel positions. For an end-to-end check, send a valid payload from the ESP32 and query the relevant measurements by plant ID and device ID. The test itself does not contact InfluxDB.

### Grafana

- Add an InfluxDB data source configured for Flux with a scoped read token, the plantcare bucket, and your local organization. Import grafana/dashboard.json using a Grafana version that supports the V2 Resource model.
- Use the data-source UID plantcare-influxdb or replace that reference in the export with your actual UID. If the import does not offer source mapping, edit the panel queries and select your source.
- Check the Last Data Received tile alongside the status cards. The matrix reads the latest value for each pixel field in the selected range. It needs new points from the pixel-enabled HTTPS flow. The summary-only flow and older points cannot fill this panel. The status rules are trial thresholds. Missing data can display the base color, so read the status text and age.

See the [Node-RED environment variable guide](https://nodered.org/docs/user-guide/environment-variables), [Grafana dashboard JSON formats](https://grafana.com/docs/grafana/latest/visualizations/dashboards/build-dashboards/view-dashboard-json-model/), and [Grafana InfluxDB source troubleshooting](https://grafana.com/docs/grafana/latest/datasources/influxdb/troubleshooting/) for the relevant configuration screens.

## Report and presentation

- [Final report PDF](docs/report/PlantCare-Final-Report.pdf) and [PowerPoint deck](docs/PlantCare-IoT-Final-Presentation.pptx).
- Build the LaTeX report from **docs/report** with **pdflatex -interaction=nonstopmode -halt-on-error PlantCare-Final-Report.tex** twice, or upload that directory to Overleaf and select pdfLaTeX.
- The 24-hour capture shows two peaks around the reported 08:30 and 17:00 waterings on 5 November. The first followed rain and reached roughly 70%. The second reached the index ceiling of 100%. The graph cannot separate rain from manual watering.
- The two-day view shows a thermal gap around midday on 4 November while soil data continues. Its history table shows 2,794 rows. The earlier Explore CSV contains 5,860 unique minute rows, with both sensor families present in 5,844 rows. These are separate time selections.
- Live proof of the reported 180-day retention and daily Cron exports, and the measured cloud-only comparison, remain outstanding.

### Recorded demonstration

| Time | What to show |
| --- | --- |
| 0:00–1:00 | Explain the plant care problem and why the ESP32 shows readings locally. |
| 1:00–2:00 | Walk through the four layers and follow the HTTPS path. |
| 2:00–4:30 | Show a real change at the sensor, follow that reading through Node-RED HTTPS ingest and InfluxDB, and show the new point in Grafana. Match the device and time across the views, then point out the thermal matrix. |
| 4:30–6:00 | Explain what worked, the measured watering response and CSV counts, and the measurements still outstanding. |
| 6:00–7:00 | State the limits and the next experiment. |

The [slide-by-slide script](docs/PlantCare-Presentation-Script.md) gives the handoff and live demo cues. Both presenters speak in the recording. The Week 16 oral slot is separate. Use about five minutes for the architecture and headline result and about five minutes for questions. Each member should answer a question. Add the recording link to the course submission when available.

Deployment secrets belong in local configuration. The repository contains example credentials and sanitized JSON exports.
