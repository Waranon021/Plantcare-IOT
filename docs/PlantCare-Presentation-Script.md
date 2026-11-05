# PlantCare IoT presentation script

ประโยคพูดเป็นภาษาอังกฤษ ส่วนคำกำกับและการสลับหน้าจอเป็นภาษาไทย เวลาเป็นจุดซ้อมสำหรับทีม ปรับตามจังหวะการพูดได้

## รอบนำเสนอสดประมาณ 5 นาที

เปิดสไลด์ 1 → 2 → 4 → 7 → 10 → 11 → 12 จากนั้นเข้าสู่ Q&A ประมาณ 5 นาที ใช้สไลด์ 3, 5, 6, 8, 9 และ 13 เป็นสไลด์สำรอง ไม่ต้องเปิดวิดีโอหรือทำเดโมสดซ้ำในรอบนี้ ให้ทั้งสองคนตอบอย่างน้อยคนละหนึ่งคำถาม

| เวลา | สไลด์ | ผู้พูด |
| --- | --- | --- |
| 0:00–0:25 | 1 — PlantCare IoT | Jirapat |
| 0:25–1:00 | 2 — Why monitor soil? | Jirapat |
| 1:00–2:15 | 4 — Four layers of PlantCare | Waranon |
| 2:15–3:05 | 7 — Grafana and the lab platform | Waranon |
| 3:05–4:15 | 10 — Measured sensor response and data gaps | Waranon |
| 4:15–4:50 | 11 — What PlantCare shows | Jirapat |
| 4:50–5:00 | 12 — Thank you | Jirapat |

## บทพูดรายสไลด์

### Slide 1 — PlantCare IoT · Jirapat

“Good afternoon. I’m Jirapat, and this is my teammate Waranon. Our project, PlantCare IoT, monitors soil wetness and surface temperature around one potted plant. We wanted the owner to see how the readings change after watering and to spot periods when data stops arriving.”

### Slide 2 — Why monitor soil? · Jirapat

“The soil surface can look dry while the roots still have water. Heat can also vary around the pot. A cloud-only setup depends on the connection even when we’re standing beside the plant. Our ESP32 samples about every two seconds and shows readings on the LCD. It attempts a remote upload about once a minute. Waranon will show where that data goes.”

### Slide 3 — Related work and protocol choice · สำรอง · Waranon

“Kim and colleagues used a wireless network to control field irrigation. Abdelmoneim and colleagues used an ESP32 to log soil water potential locally and send it to ThingSpeak. Our project monitors one pot and leaves watering to the owner. IIRA helped us separate how the owner uses the system, what each layer does, and where it runs.”

### Slide 4 — Four layers of PlantCare · Waranon

“Starting on the left, the Soil Moisture Sensor and the AMG8833 thermal sensor connect to an ESP32. It smooths the soil signal and keeps the latest readings for the LCD. At the network layer, it sends JSON over Wi-Fi and HTTPS. Node-RED validates the request on the lab platform and sends the soil and thermal data to InfluxDB. Grafana then reads that data for the dashboard and historical review. The MQTT branch below is a test subscriber. It ends at the Node-RED Debug panel. The live readings on this diagram follow the HTTPS path.”

คิวชี้ภาพ ให้ชี้สี่ชั้นจากซ้ายไปขวา แล้วชี้บรรทัด MQTT แยกด้านล่าง

### Slide 5 — HTTPS ingest and MQTT debug · สำรอง · Waranon

“The ESP32 posts to /plantcare/ingest. The request identifies the plant and device and includes whichever sensor sections are available. A 202 response means Node-RED accepted it for processing. To check storage, we look for the matching reading in InfluxDB. The MQTT subscription uses a wildcard to match test topics such as soil. Its output goes to Debug.”

### Slide 6 — The device and soil index · สำรอง · Jirapat

“The ESP32 reads the soil sensor as an ADC number and averages 20 readings. It then smooths them before calculating the wetness index. Our drier and wetter reference values are 2614 and 2274. A filtered reading of 2444 is halfway between them, so it gives an index of 50 percent. This is a relative scale for this pot. It does not tell us the percentage of water in the soil.”

### Slide 7 — Grafana and the lab platform · Waranon

“This is the full dashboard. Across the top are the latest soil and thermal readings, their status, and how long ago data arrived. Below them are the trends and the data availability timeline. The new panel at the bottom left is an eight-by-eight thermal matrix. Each cell shows the temperature measured by one sensor pixel. It can include nearby surfaces in the sensor’s view, so we read it as a temperature pattern.”

คิวชี้ภาพ ชี้แถวบนก่อน แล้วชี้ Heatmap มุมซ้ายล่าง เทียบกับกราฟ Thermal ที่แสดงการเปลี่ยนแปลงตามเวลา

### Slide 8 — Device access · สำรอง · Waranon

“The ESP32 verifies the server certificate before sending data. Node-RED also checks a shared key in the request header. The prototype uses a static key, so there’s no automatic token rotation. The repository has an example header so each installation can keep its real credentials local.”

### Slide 9 — What we tested and what remains · สำรอง · Jirapat

“We reviewed four watering trials and two Grafana views, audited a CSV export, and ran nine offline Node-RED checks. We haven’t run the matched cloud-only performance trial. The tools we would use are sysbench for CPU and memory, dd for disk throughput, and iPerf3 for link behavior. We would measure application latency, actual traffic, and power separately.”

### Slide 10 — Measured sensor response and data gaps · Waranon

“Across four paired watering observations, the raw soil reading fell by an average of 340.5 ADC counts. The new dashboard also shows peaks around two reported waterings on 5 November. The morning peak was about 70 percent after rain. The evening peak reached the index ceiling of 100 percent. Our earlier CSV audit counted 5,860 unique minute rows. Its longest gap was 16 hours and 7 minutes. In the newer two-day view, we can also see a thermal gap while soil readings continued.”

คิวชี้ภาพ กราฟแท่งเป็นจำนวนแถวของ CSV เดิม ส่วนเหตุการณ์รดน้ำและช่วงข้อมูลขาดดูภาพขยายได้ที่สไลด์ 13

### Slide 11 — What PlantCare shows · Jirapat

“We can follow real sensor readings from the pot to the dashboard. The trial covers one pot, and its status thresholds still need validation. Our next step is to repeat measured watering across several pots and compare against an independent soil moisture reading. That would help us judge whether the index is useful in other setups.”

### Slide 12 — Thank you · Jirapat

“Thank you. We’re happy to take questions about the architecture or the results.”

### Slide 13 — Watering and data gap details · สำรอง · Waranon

“On the left, the wetness index rose and the ADC signal dipped around the two reported watering times. The morning event followed rain, so we can’t separate the effects. On the right, the red band marks missing thermal points around midday on 4 November. The soil row stays green during that interval. We can see which readings stopped arriving, but the graph cannot tell us why.”

## วิดีโอสาธิตประมาณ 7 นาที

| เวลา | ผู้พูดและสิ่งที่โชว์ |
| --- | --- |
| 0:00–1:00 | **Jirapat** เปิดสไลด์ 1–2 และอธิบายปัญหา ให้เห็นว่าหน้าจอท้องถิ่นยังมีประโยชน์เมื่อส่งข้อมูลขึ้นระบบไม่ได้ |
| 1:00–2:00 | **Waranon** เปิดสไลด์ 4 ไล่เส้นทาง HTTPS จาก ESP32 ถึง Grafana และชี้ว่า MQTT เป็น Debug branch |
| **2:00–4:30** | **สลับจากสไลด์ไปเดโมจริง** ทำตามคิวด้านล่าง ช่วงนี้ควรเห็นอุปกรณ์และข้อมูลจริงที่อัปเดตบนหน้าจอ |
| 4:30–6:00 | **Waranon** กลับมาสไลด์ 10 และเปิดสไลด์ 13 ได้ อธิบายผลที่วัดได้ ช่วงข้อมูลขาด และการทดสอบเปรียบเทียบที่ยังไม่มีผล |
| 6:00–7:00 | **Jirapat** ใช้สไลด์ 11–12 สรุปข้อจำกัดและการทดลองต่อไป |

### คิวเดโมจริง 2:00–4:30

1. **2:00–2:30 — Jirapat, กล้องที่อุปกรณ์และหน้า ESP32/LCD.** “This is the physical Soil Moisture Sensor and the ESP32. Here is the current reading. I’ll change the soil condition and we’ll follow the next reading.” ให้เห็นค่าก่อนเปลี่ยน แล้วใช้วิธีที่ซ้อมไว้กับกระถางเดโม เช่น เติมน้ำปริมาณเล็กน้อยตามความเหมาะสม
2. **2:30–3:35 — Waranon, เปิด Node-RED และผล query ของ InfluxDB ที่เตรียมไว้.** “The ESP32 sends its latest reading on the HTTPS route. Here is the matching device and time in InfluxDB.” ระหว่างรอรอบส่งประมาณหนึ่งนาที ให้ชี้เส้นทาง HTTPS และอธิบายว่า HTTP 202 บอกการรับคำขอ จากนั้นโชว์แถวจริงที่ตรงกับอุปกรณ์และเวลาที่สังเกต
3. **3:35–4:30 — Waranon, เปิด Grafana แบบสด.** “This new point is from the same device. You can see the change in the soil trend and the time of the last reading. The thermal matrix shows the latest available temperatures across the sensor’s field of view.” ใช้ Refresh และชี้เวลาของจุดใหม่ ตรวจว่า Heatmap แสดงข้อมูลจาก bucket จริงก่อนอัด

ซ้อมหนึ่งรอบก่อนอัดและเตรียมหน้าต่างที่จะสลับให้พร้อม ปิดบัง Wi-Fi credentials, API keys และข้อมูลการเชื่อมต่อ หากข้อมูลไม่อัปเดต ให้แก้สาเหตุและอัดใหม่ ภาพที่บันทึกไว้ใช้ประกอบการอธิบายผล แต่ช่วง 2:00–4:30 ต้องแสดงระบบจริงตาม rubric
