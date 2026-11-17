# Offline evidence

Open `index.html` directly in a browser. Keep the `assets` folder beside it. No web server, internet, Grafana, or InfluxDB is required.

| File | Purpose |
| --- | --- |
| `index.html` | Offline evidence viewer with explanations and original images. |
| `assets/grafana-overview.png` | Saved full dashboard, identified in the report as the 24-hour view on 5 November 2026. |
| `assets/grafana-watering.png` | Saved wetness and ADC plots around the two reported watering events. |
| `assets/grafana-two-days.png` | Saved soil/thermal availability timeline around 4 November. |

These are historical screenshots from the supplied report folder. Their values, status colors, and "last received" labels are frozen at capture time. They are not current readings. The viewer does not synthesize data, restore InfluxDB, or provide CSV export.

The supplied Grafana recovery JSON still queries InfluxDB. Use this offline viewer when that database is unavailable. No raw CSV was present in the supplied local project folder, so a full time-series database backup is not included.
