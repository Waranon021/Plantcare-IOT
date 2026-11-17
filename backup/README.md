# Offline supporting evidence

Open `index.html` directly in a browser. Keep the `assets` folder beside it. No web server, internet, Grafana, or InfluxDB is required.

| File | Purpose |
| --- | --- |
| `index.html` | Offline evidence viewer with the project overview and explanation of the local recovered dashboard. |
| `assets/grafana-overview.png` | Saved full dashboard, identified in the report as the 24-hour view on 5 November 2026. |
| `assets/grafana-local-history.png` | Team-supplied local recovered dashboard showing 3-17 November 2026. Only the browser address bar was cropped out for privacy. |

## Why the local view was prepared

Shortly before submission, the team experienced a problem with the shared Grafana environment. A local Grafana view was prepared as supporting evidence that the sensor data had been collected and stored in InfluxDB and could still be queried.

The team's records extend back to 29 October, as documented in the final report's earlier CSV audit. The attached local view displays 3-17 November, so the earlier start date is supported separately rather than visible in this screenshot. The combined evidence supports a collection period of more than two weeks, with gaps.

The team observed interruptions in the early period and more consistent recording later, although some gaps remain visible. The connectivity panel reports telemetry freshness, not a direct measurement of Wi-Fi state.

The team also observed wetness decreasing as soil dried and increasing after watering. From around 10 November, several soil-sensor recalibrations were performed because of calibration issues. Later fluctuations and saturated values can therefore reflect changes in calibration as well as soil conditions. These trends are supporting observations, not independent validation of plant health. Isolated sensor anomalies require separate investigation.

The environment problem and recalibration timeline are the team's account. The 29 October start is documented in the report. The full stored dataset was not independently re-audited for this page.

## Offline limits

Values, status colors, and "last received" labels are frozen at capture time. They are not current readings. This viewer does not synthesize data, restore InfluxDB, or provide CSV export.

The recovered Grafana JSON still queries InfluxDB. Use this offline viewer when that database is unavailable. No raw CSV was supplied in the local project folder, so this is a submission evidence fallback rather than a full time-series database backup.

The two-day and watering images are no longer included in `backup/assets`. Their original copies remain in `report/figures` because the supplied manuscript references them.
