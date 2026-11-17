// คัดลอกเป็น credentials.h แล้วใส่ค่าของเครื่องตนเอง ไฟล์จริงถูกกันออกจาก Git
// sketch หลักอ่าน Wi-Fi และ HTTPS endpoint/key จากไฟล์นี้; key ต้องตรงกับ Node-RED
#pragma once

const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
// ปล่อยว่างไว้จนกว่าจะตั้ง key แบบสุ่มอย่างน้อย 32 ตัวอักษร; firmware จะไม่ส่งเมื่อยังว่าง
const char* CLOUD_API_KEY = "";
const char* CLOUD_URL = "https://example.invalid/plantcare/ingest";
