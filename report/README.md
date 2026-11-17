# PlantCare final report handoff

The manuscript follows the IEEE Access LaTeX template. The report uses a new four-layer diagram, the verified Node-RED HTTPS branch, the full 24-hour dashboard with an 8×8 thermal matrix, and close-up plots of the two watering events and the two-day gap. The report checks the title and sustainability problem, maps the design to IIRA, compares related work and protocols, documents the four layers and HTTP contract, and traces the hardware-to-dashboard workflow.

Build with pdfLaTeX twice from this folder. Use PlantCare-Final-Report.tex as the main document. The results and bibliography are in results.tex and references.tex.

The revised manuscript verifies the configured Node-RED ingest and Grafana query behavior from the supplied JSON files. Nine offline tests exercised the ingest function. The tests do not measure database writes, service uptime, or live performance. The two-day dashboard shows 2,794 rows for its selected view. The 5,860-row Explore CSV audit covers a separate export.

Before submission, attach the actual demo video link. Add live bucket and Cron evidence and a measured cloud-only comparison if available. The manuscript describes those gaps without placeholder text.

Project source repository: https://github.com/Waranon021/Plantcare-IOT

## Repository preparation note

This folder retains the supplied manuscript, PDF, figures, template, and fonts. Presentation slides and speaking/demo scripts are submitted separately and are excluded from Git.

The current device sketch differs from the reported trial configuration: its upload interval is 2,000 ms and its soil reference pair is 2492/2152 ADC. The manuscript and existing PDF describe a 60,000 ms upload interval and 2614/2274 ADC. See the root README for the distinction. The existing PDF was not rebuilt or changed during repository preparation; compiling the supplied LaTeX may produce a newer rendering than that PDF.

The old manuscript's nine-test claim describes its historical verification. The old repository test file is not carried into this replacement source set. Refer to current validation notes for checks of the supplied flow; those checks do not prove successful live database writes.
