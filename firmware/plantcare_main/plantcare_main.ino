/*
 * ============================================================
 * PlantCare IoT - Portulaca Thermal and Soil Monitoring
 * ============================================================
 *
 * ไฟล์: plantcare_main.ino
 * อุปกรณ์หลัก: ESP32-WROOM-32 บน NECTEC I-KIT Version 2.1
 * ต้นไม้ทดลอง: คุณนายตื่นสาย (Portulaca)
 *
 * วัตถุประสงค์:
 * 1. อ่านค่าความชื้นดินจาก Capacitive Soil Moisture Sensor
 * 2. อ่านภาพความร้อน 8x8 Pixels จาก AMG8833
 * 3. คำนวณ Relative Wetness Index (%) จากผลสำรวจจริง
 * 4. แสดง Soil และ Thermal บนหน้า Web Dashboard
 * 5. แสดงข้อมูลบน LCD 16x2 ของ I-KIT
 * 6. แสดงข้อมูลผ่าน Arduino Serial Monitor
 * 7. เปิด HTTP JSON สำหรับ Local Dashboard
 * 8. ส่ง Soil + Thermal JSON ขึ้น Node-RED HTTPS ทุก 60 วินาที
 *
 * การต่อเซนเซอร์:
 *
 * Soil Moisture Sensor:
 *   VCC    -> 3V3
 *   GND    -> GND
 *   Signal -> GPIO34
 *
 * AMG8833:
 *   VIN -> 3V3 (ตรวจว่า Breakout รองรับ 3.3V)
 *   GND -> GND
 *   SDA -> GPIO21
 *   SCL -> GPIO22
 *
 * LCD:
 *   ใช้จอ LCD1602 ที่ติดอยู่บน I-KIT
 *   โปรแกรมรองรับ I2C Address 0x27 และ 0x3F
 *   ต้องตรวจว่า LCD ใช้ Bus และแรงดันที่เข้ากันได้
 *
 * ------------------------------------------------------------
 * ข้อมูล Soil Calibration จากภาพวันที่ 20-22 October 2026
 * ------------------------------------------------------------
 *
 * ก่อนรดน้ำ 4 รอบ:
 *   2770, 2622, 2559, 2505
 *   Average = 2614 ADC
 *
 * หลังรดน้ำทันที 4 รอบ:
 *   2307, 2238, 2226, 2323
 *   Average = 2273.5 ADC
 *
 * ค่าอ้างอิง:
 *   ADC 2614 -> 0% Relative Wetness Index
 *   ADC 2274 -> 100% Relative Wetness Index
 *
 * สำคัญ:
 * - 0% ไม่ใช่ดินแห้งสนิท
 * - 100% ไม่ใช่ดินอิ่มน้ำ 100%
 * - เป็นดัชนีเปรียบเทียบช่วงก่อนและหลังรดน้ำเท่านั้น
 * - ไม่ใช้ค่า Index สั่งรดน้ำอัตโนมัติ
 * - ต้องเก็บ Raw ADC ควบคู่กับ Index เสมอ
 *
 * Software Architecture:
 *
 *   Soil Sensor ----\
 *                    ESP32 -> LCD1602
 *   AMG8833 --------/      -> Serial Monitor
 *                          -> Web Dashboard
 *                          -> HTTP JSON
 *
 * HTTP Endpoints:
 *   /         หน้า Web Dashboard
 *   /soil     ค่า Soil แบบ JSON
 *   /thermal  Thermal Pixels 64 ค่าแบบ JSON
 *   /status   สถานะ ESP32 และเซนเซอร์
 *
 * เชื่อม Node-RED HTTPS Ingest โดยตรง (ไม่ใช่ MQTT Client)
 * Node-RED ประมวลผล JSON ก่อนบันทึก InfluxDB
 * HTTP Server บน ESP32 ไม่มี Login ใช้เฉพาะเครือข่ายที่ไว้ใจได้
 *
 * คำเตือน: เติม Wi-Fi Password และ Cloud API Key เฉพาะในเครื่องตัวเอง
 * อย่า Commit/Push ไฟล์ที่มีรหัสลับไปยัง Git Repository
 */

//-- 1. Libraries
// ------------------------------------------------------------

// ไลบรารีพื้นฐานของ Arduino ESP32
#include <Arduino.h>
#include "credentials.h"

// ใช้เชื่อมต่อเครือข่าย Wi-Fi
#include <WiFi.h>

// ใช้สร้าง Web Server ที่ทำงานบน ESP32 เอง
#include <WebServer.h>

// ใช้สื่อสารกับอุปกรณ์บน I2C Bus
#include <Wire.h>

// ไลบรารีสำหรับ AMG8833 Thermal Sensor
#include <Adafruit_AMG88xx.h>

// ไลบรารีสำหรับ LCD1602 แบบ I2C
#include <LiquidCrystal_I2C.h>

// ใช้คำนวณค่าทางคณิตศาสตร์
#include <math.h>
#include <time.h>
#include <string.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>


//-- 2. Wi-Fi Configuration
// ------------------------------------------------------------

// ใส่ Wi-Fi ของหอพักในเครื่องตัวเองเท่านั้น

// API Key ต้องอยู่ภายใน double quotes (ไม่ใส่เครื่องหมายซ้อนกัน)
// ค่านี้ต้องตรงกับ PLANTCARE_INGEST_KEY ที่ตั้งใน Node-RED

const unsigned long CLOUD_UPLOAD_INTERVAL_MS = 60000UL;


//-- 3. Hardware Configuration
// ------------------------------------------------------------

// Soil Moisture ใช้ GPIO34 ซึ่งเป็น ADC1
// ทำให้สามารถอ่าน Analog ขณะเปิด Wi-Fi ได้
const int SOIL_PIN = 34;

// AMG8833 ใช้ I2C ผ่าน GPIO21 และ GPIO22
const int I2C_SDA_PIN = 21;
const int I2C_SCL_PIN = 22;

// เปิดระบบ LCD1602 ที่ติดตั้งอยู่บน I-KIT
// หากตรวจแล้วว่าจอไม่ได้ใช้ I2C Bus นี้
// ให้เปลี่ยนกลับเป็น false ก่อนใช้งาน
const bool ENABLE_LCD = true;


//-- 4. Soil Calibration from Observations
// ------------------------------------------------------------

// ค่าเฉลี่ยก่อนรดน้ำ
// เป็นด้านที่ชื้นน้อยกว่าในช่วงที่เก็บข้อมูล
// ไม่ได้หมายถึงดินแห้งสนิท
const float SOIL_BEFORE_WATER_REF = 2614.0f;

// ค่าเฉลี่ยหลังรดน้ำทันที
// เป็นด้านที่ชื้นมากกว่าในช่วงที่เก็บข้อมูล
// ไม่ได้หมายถึงดินมีน้ำ 100% ทางกายภาพ
const float SOIL_AFTER_WATER_REF = 2274.0f;

// จำนวนการอ่าน ADC ต่อหนึ่งรอบ
// การเฉลี่ยหลายตัวอย่างช่วยลด Noise ระยะสั้น
const int SOIL_SAMPLE_COUNT = 20;

// Exponential Moving Average
// ค่า 0.25 ให้น้ำหนักข้อมูลใหม่ 25%
// และข้อมูลสะสมเดิม 75%
const float SOIL_EMA_ALPHA = 0.25f;


//-- 5. Sampling and Display Timing
// ------------------------------------------------------------

// อ่าน Soil และ Thermal ทุก 2 วินาที
const unsigned long SAMPLE_INTERVAL_MS = 2000;

// อัปเดต LCD และ Serial ทุก 2 วินาที
const unsigned long DISPLAY_INTERVAL_MS = 2000;

// สลับหน้าจอ LCD ทุก 6 วินาที
// เพราะ LCD มีเพียง 16 ตัวอักษร 2 บรรทัด
const unsigned long LCD_PAGE_INTERVAL_MS = 6000;

// ลองเชื่อม Wi-Fi หรือ AMG8833 ใหม่ทุก 10 วินาที
const unsigned long RETRY_INTERVAL_MS = 10000;


//-- 6. Global Objects
// ------------------------------------------------------------

// ออบเจ็กต์สำหรับ Thermal Sensor
Adafruit_AMG88xx amg;

// Web Server บน HTTP Port 80
WebServer server(80);

// เตรียม LCD สอง Address ที่พบบ่อย
// โปรแกรมจะเลือกอันที่ตรวจพบจริง
LiquidCrystal_I2C lcd27(0x27, 16, 2);
LiquidCrystal_I2C lcd3F(0x3F, 16, 2);

// Pointer สำหรับจอ LCD ที่เริ่มต้นสำเร็จ
// หากยังไม่พบ LCD จะมีค่า nullptr
LiquidCrystal_I2C* lcd = nullptr;


//-- 7. Global Sensor Data
// ------------------------------------------------------------

// Soil ADC เฉลี่ยจากตัวอย่างล่าสุด 20 ค่า
int soilRaw = 0;

// ค่า ADC หลังกรองด้วย EMA
float soilFiltered = 0.0f;

// ค่าดัชนีความชื้นสัมพัทธ์ 0-100%
float relativeWetnessPct = 0.0f;

// ตรวจว่าการอ่าน Soil ใช้งานได้หรือไม่
bool soilReady = false;

// ตรวจว่า EMA เริ่มทำงานแล้วหรือยัง
bool soilFilterReady = false;

// AMG8833 มีข้อมูล 8 x 8 = 64 Pixels
float thermalPixels[64] = {};

// อุณหภูมิต่ำสุดและสูงสุดของ Thermal Frame
float thermalMin = 0.0f;
float thermalMax = 0.0f;

// สถานะการเชื่อม AMG8833 และการอ่านข้อมูล
bool amgConnected = false;
bool thermalReady = false;

// I2C Address ที่ตรวจพบสำหรับ AMG8833
uint8_t amgAddress = 0;

// ตรวจว่าเริ่ม HTTP Server แล้วหรือยัง
bool httpStarted = false;

// ตัวจับเวลาการทำงานแต่ละส่วน
unsigned long lastSampleAt = 0;
unsigned long lastDisplayAt = 0;
unsigned long lastAmgRetryAt = 0;
unsigned long lastWiFiRetryAt = 0;
unsigned long lastCloudAttemptAt = 0;


//-- 8. I2C Helper Functions
// ------------------------------------------------------------

// ตรวจว่ามีอุปกรณ์ตอบสนองที่ I2C Address นี้หรือไม่
bool hasI2CDevice(uint8_t address) {
  Wire.beginTransmission(address);

  return Wire.endTransmission() == 0;
}


// สแกนอุปกรณ์ I2C ทั้ง Bus
// ใช้ดูว่า LCD และ AMG8833 ถูกตรวจพบหรือไม่
void scanI2CDevices() {
  Serial.println("[I2C] Scanning devices...");

  int found = 0;

  for (uint8_t address = 0x08; address <= 0x77; address++) {
    if (hasI2CDevice(address)) {
      Serial.printf(
        "[I2C] Found device at 0x%02X\n",
        address
      );

      found++;
    }
  }

  if (found == 0) {
    Serial.println("[I2C] No devices found");
  }
}


//-- 9. Initialize AMG8833
// ------------------------------------------------------------

// เริ่ม AMG8833 โดยทดลอง Address 0x69 ก่อน
// หากไม่สำเร็จจะลอง Address 0x68
bool initializeAMG8833() {

  if (
    hasI2CDevice(0x69) &&
    amg.begin(0x69, &Wire)
  ) {
    amgAddress = 0x69;

    Serial.println("[AMG8833] Connected at 0x69");
    return true;
  }

  if (
    hasI2CDevice(0x68) &&
    amg.begin(0x68, &Wire)
  ) {
    amgAddress = 0x68;

    Serial.println("[AMG8833] Connected at 0x68");
    return true;
  }

  amgAddress = 0;

  Serial.println("[AMG8833] Initialization failed");
  Serial.println("[AMG8833] Check I2C and power");

  return false;
}


//-- 10. Initialize LCD1602
// ------------------------------------------------------------

// ฟังก์ชันนี้แก้ปัญหา ENABLE_LCD = false ในโค้ดเดิม
// โดยทดลองค้นหาจอ LCD บน I2C Bus
//
// ถ้าพบ:
//   - เริ่มต้นจอ
//   - เปิด Backlight
//   - แสดงข้อความเริ่มต้น
//
// ถ้าไม่พบ:
//   - รายงานผ่าน Serial Monitor
//   - ระบบ Soil, Thermal และ Web ยังทำงานต่อ
void initializeLCD() {

  if (!ENABLE_LCD) {
    Serial.println("[LCD] Disabled in configuration");
    return;
  }

  if (hasI2CDevice(0x27)) {
    lcd = &lcd27;
    Serial.println("[LCD] Detected at 0x27");

  } else if (hasI2CDevice(0x3F)) {
    lcd = &lcd3F;
    Serial.println("[LCD] Detected at 0x3F");

  } else {
    Serial.println("[LCD] Not detected");
    Serial.println("[LCD] Check I2C address and wiring");
    return;
  }

  // เริ่มการทำงานของจอ LCD
  lcd->init();

  // เปิดไฟ Backlight ของจอ
  lcd->backlight();

  // แสดงข้อความเริ่มระบบบนบรรทัดแรก
  lcd->setCursor(0, 0);
  lcd->print("PlantCare IoT");

  // แสดงข้อความเริ่มระบบบนบรรทัดที่สอง
  lcd->setCursor(0, 1);
  lcd->print("Starting...");
}


//-- 11. Read Soil ADC
// ------------------------------------------------------------

// อ่าน ADC 20 ครั้งและหาค่าเฉลี่ย
// ช่วยลด Noise เมื่อค่า ADC แกว่งระยะสั้น
//
// ค่าที่คืนกลับยังเป็น Raw ADC
// ไม่ใช่เปอร์เซ็นต์ปริมาณน้ำในดิน
int readSoilAverage() {
  unsigned long total = 0;

  for (int i = 0; i < SOIL_SAMPLE_COUNT; i++) {
    total += analogRead(SOIL_PIN);

    // เว้นช่วง 3 ms ระหว่างแต่ละตัวอย่าง
    delay(3);
  }

  // ทำการหารเพื่อหาค่าเฉลี่ย
  return (int)(total / SOIL_SAMPLE_COUNT);
}


// ตรวจค่าที่ใกล้ขอบ ADC มากผิดปกติ
// ไม่ได้พิสูจน์ว่าเซนเซอร์เสียแน่นอน
bool isSoilReadingUsable(int raw) {
  return raw > 10 && raw < 4085;
}


//-- 12. Calculate Relative Wetness Index
// ------------------------------------------------------------

// สูตรคำนวณ:
//
// Relative Wetness Index (%) =
//  (BeforeWaterReference - CurrentADC)
//  ----------------------------------- x 100
//  (BeforeWaterReference - AfterWaterReference)
//
// ADC ต่ำลง -> ดัชนีเพิ่มขึ้น
// ADC สูงขึ้น -> ดัชนีลดลง
//
// ช่วงอ้างอิงนี้ได้จากการสังเกต 4 รอบก่อนและหลังรดน้ำ
// ไม่ใช่ Calibration ของ Volumetric Water Content
float calculateRelativeWetness(float currentADC) {

  const float referenceRange =
    SOIL_BEFORE_WATER_REF - SOIL_AFTER_WATER_REF;

  // ป้องกันการหารด้วยศูนย์
  if (referenceRange <= 0.0f) {
    return 0.0f;
  }

  float index =
    (SOIL_BEFORE_WATER_REF - currentADC) /
    referenceRange * 100.0f;

  // จำกัดดัชนีที่แสดงให้อยู่ระหว่าง 0-100%
  // แต่ยังเก็บ Raw ADC จริงไว้เพื่อศึกษาย้อนหลัง
  return constrain(index, 0.0f, 100.0f);
}


//-- 13. Soil Status Classification
// ------------------------------------------------------------

// สร้างข้อความภาษาอังกฤษสำหรับเว็บและ Serial
// ข้อความนี้อธิบายตำแหน่งในช่วงทดลอง
// ไม่ใช่คำสั่งว่าต้นไม้ต้องรดน้ำทันที
const char* getSoilStatus() {

  if (!soilReady || !soilFilterReady) {
    return "Sensor reading unavailable";
  }

  // ค่า ADC ต่ำกว่าค่าเฉลี่ยหลังรดน้ำ
  if (soilFiltered < SOIL_AFTER_WATER_REF) {
    return "Wetter than observed reference";
  }

  // ค่า ADC สูงกว่าค่าเฉลี่ยก่อนรดน้ำ
  if (soilFiltered > SOIL_BEFORE_WATER_REF) {
    return "Below observed wetness range - check soil";
  }

  if (relativeWetnessPct >= 80.0f) {
    return "High relative wetness - recently watered";
  }

  if (relativeWetnessPct >= 50.0f) {
    return "Moister than pre-watering baseline";
  }

  if (relativeWetnessPct >= 20.0f) {
    return "Intermediate observed wetness";
  }

  return "Near pre-watering baseline - monitor soil";
}


// คืนรหัสสถานะสั้น ๆ สำหรับใช้บน LCD
// เพราะจอ LCD1602 มีพื้นที่เพียง 16 ตัวอักษร
const char* getSoilShortStatus() {

  if (!soilReady || !soilFilterReady) {
    return "NO SENSOR";
  }

  if (soilFiltered < SOIL_AFTER_WATER_REF) {
    return "ABOVE WET REF";
  }

  if (soilFiltered > SOIL_BEFORE_WATER_REF) {
    return "CHECK SOIL";
  }

  if (relativeWetnessPct >= 80.0f) {
    return "HIGH INDEX";
  }

  if (relativeWetnessPct >= 50.0f) {
    return "MOIST RANGE";
  }

  if (relativeWetnessPct >= 20.0f) {
    return "MID RANGE";
  }

  return "LOW INDEX";
}


//-- 14. Read AMG8833 Thermal Frame
// ------------------------------------------------------------

// AMG8833 มี Thermal Pixels จำนวน 64 จุด
// อ่านค่าแล้วหาต่ำสุดและสูงสุด
//
// Frame ทั้ง 64 ค่าจะถูกนำไปแสดงบนหน้าเว็บ
// และ Min/Max จะแสดงบน LCD และ Serial Monitor
bool readThermalFrame() {

  if (!amgConnected) {
    return false;
  }

  // ตรวจว่า Sensor ยังตอบสนอง I2C
  if (!hasI2CDevice(amgAddress)) {
    amgConnected = false;

    Serial.println("[AMG8833] Connection lost");
    return false;
  }

  float newPixels[64];

  // อ่านข้อมูลจาก AMG8833
  amg.readPixels(newPixels);

  float minimum = INFINITY;
  float maximum = -INFINITY;

  // ตรวจค่าทุก Pixel ก่อนใช้งาน
  for (int i = 0; i < 64; i++) {

    float value = newPixels[i];

    if (!isfinite(value)) {
      Serial.println("[AMG8833] Invalid pixel");
      return false;
    }

    if (value < minimum) {
      minimum = value;
    }

    if (value > maximum) {
      maximum = value;
    }
  }

  // คัดลอก Frame ที่อ่านได้ลงตัวแปรกลาง
  for (int i = 0; i < 64; i++) {
    thermalPixels[i] = newPixels[i];
  }

  thermalMin = minimum;
  thermalMax = maximum;

  return true;
}


//-- 15. Sample Soil and Thermal Sensors
// ------------------------------------------------------------

// อ่านเซนเซอร์ทั้งสองตัวในรอบเดียวกัน
//
// แนวคิด:
// อ่านหนึ่งครั้ง -> เก็บข้อมูลกลาง -> กระจายไปหลาย Output
//
// ช่วยให้ Web, LCD และ Serial ใช้ค่าเดียวกัน
// และไม่จำเป็นต้องอ่าน Sensor ใหม่ทุก HTTP Request
void sampleSensors() {

  // อ่านค่า Soil ADC เฉลี่ยจาก 20 ตัวอย่าง
  soilRaw = readSoilAverage();

  // ตรวจความสมเหตุสมผลเบื้องต้น
  soilReady = isSoilReadingUsable(soilRaw);

  if (soilReady) {

    if (!soilFilterReady) {

      // รอบแรกกำหนด Filter เท่ากับ ADC ปัจจุบัน
      soilFiltered = soilRaw;
      soilFilterReady = true;

    } else {

      // ใช้ EMA ลดการแกว่งระหว่างรอบ
      soilFiltered =
        SOIL_EMA_ALPHA * soilRaw +
        (1.0f - SOIL_EMA_ALPHA) * soilFiltered;
    }

    // คำนวณดัชนีความชื้นจาก ADC ที่กรองแล้ว
    relativeWetnessPct =
      calculateRelativeWetness(soilFiltered);

  } else {

    // หาก ADC ผิดปกติ ไม่ควรรายงาน % ว่าเป็น 0
    // เพราะ 0 อาจถูกเข้าใจผิดว่าเป็นดินแห้ง
    soilFilterReady = false;
    relativeWetnessPct = 0.0f;
  }

  // อ่านอุณหภูมิในรอบเดียวกัน
  thermalReady = readThermalFrame();
}


//-- 16. LCD Utility Functions
// ------------------------------------------------------------

// แสดงข้อความบน LCD โดยจำกัดความยาว 16 ตัวอักษร
// และลบตัวอักษรเก่าที่อาจค้างจากการอัปเดตก่อนหน้า
void printLCDLine(int row, String message) {

  if (lcd == nullptr) {
    return;
  }

  if (message.length() > 16) {
    message = message.substring(0, 16);
  }

  lcd->setCursor(0, row);
  lcd->print(message);

  // เติมช่องว่างให้ครบ 16 ตัวอักษร
  for (int i = message.length(); i < 16; i++) {
    lcd->print(' ');
  }
}


//-- 17. Update LCD1602
// ------------------------------------------------------------

// เนื่องจาก LCD มี 2 บรรทัด จึงสลับแสดงสองหน้า
//
// Page 1: ค่าความชื้นและอุณหภูมิ
//
//   IDX: 29.4%
//   T:26.3-29.5C
//
// Page 2: ค่า ADC และสถานะของดิน
//
//   ADC: 2514
//   MID RANGE
//
// แต่ละหน้าจะแสดงประมาณ 6 วินาที
void updateLCD() {

  if (lcd == nullptr) {
    return;
  }

  // ใช้เวลาจาก millis() เพื่อเลือกหน้า LCD
  bool firstPage =
    ((millis() / LCD_PAGE_INTERVAL_MS) % 2) == 0;

  if (firstPage) {

    // หน้าแรก: Relative Wetness Index
    String line1 = "IDX: NO DATA";

    if (soilReady && soilFilterReady) {
      line1 =
        "IDX: " +
        String(relativeWetnessPct, 1) +
        "%";
    }

    // หน้าแรก: ช่วงอุณหภูมิ Min/Max
    String line2 = "THERMAL NO DATA";

    if (thermalReady) {
      line2 =
        "T:" +
        String(thermalMin, 1) +
        "-" +
        String(thermalMax, 1) +
        "C";
    }

    printLCDLine(0, line1);
    printLCDLine(1, line2);

  } else {

    // หน้าที่สอง: Raw ADC
    String line1 = "ADC: NO DATA";

    if (soilReady && soilFilterReady) {
      line1 =
        "ADC: " +
        String((int)roundf(soilFiltered));
    }

    // หน้าที่สอง: สถานะภาษาอังกฤษแบบสั้น
    String line2 = getSoilShortStatus();

    printLCDLine(0, line1);
    printLCDLine(1, line2);
  }
}


//-- 18. Serial Monitor Output
// ------------------------------------------------------------

// แสดงค่า Soil, Relative Wetness Index, Thermal
// และ IP ของ ESP32 ผ่าน USB Serial Monitor
//
// ประโยชน์:
// - ตรวจสอบข้อมูลโดยไม่ต้องเปิด Browser
// - เปรียบเทียบ ADC กับ % ที่คำนวณได้
// - ตรวจสอบว่า Thermal Sensor อ่านข้อมูลจริง
void printSerialData() {

  Serial.println();
  Serial.println("=== PlantCare Sensor Reading ===");

  Serial.print("[SOIL] Average ADC: ");
  Serial.println(soilRaw);

  Serial.print("[SOIL] Filtered ADC: ");

  if (soilReady && soilFilterReady) {
    Serial.println(soilFiltered, 1);
  } else {
    Serial.println("Unavailable");
  }

  Serial.print("[SOIL] Relative Wetness Index: ");

  if (soilReady && soilFilterReady) {
    Serial.print(relativeWetnessPct, 1);
    Serial.println("%");
  } else {
    Serial.println("Unavailable");
  }

  Serial.print("[SOIL] Status: ");
  Serial.println(getSoilStatus());

  Serial.println(
    "[SOIL] Method: Experimental before/after mean"
  );

  if (thermalReady) {

    Serial.print("[AMG8833] Minimum: ");
    Serial.print(thermalMin, 2);
    Serial.println(" C");

    Serial.print("[AMG8833] Maximum: ");
    Serial.print(thermalMax, 2);
    Serial.println(" C");

  } else {

    Serial.println(
      "[AMG8833] Thermal reading unavailable"
    );
  }

  if (lcd != nullptr) {
    Serial.println("[LCD] Display enabled");
  } else {
    Serial.println("[LCD] Display unavailable");
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println("[WIFI] Disconnected");
  }
}


//-- 19. Embedded Web Dashboard
// ------------------------------------------------------------

// หน้าเว็บ HTML/CSS/JavaScript ฝังอยู่ใน Firmware
// ไม่ต้องติดตั้ง Web Server บน Laptop
//
// Browser เปิด:
//   http://device.example.invalid
//
// Thermal Panel:
//   Fetch /thermal แล้วสร้าง Heatmap
//
// Soil Panel:
//   Fetch /soil แล้วแสดง Wetness Index (%)
//   และแสดง Raw ADC เพื่อช่วยศึกษา Calibration
const char index_html[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">

<head>
<meta charset="UTF-8">
<meta name="viewport"
      content="width=device-width, initial-scale=1.0">

<title>PlantCare Thermal + Soil Monitor</title>

<style>
  * {
    box-sizing: border-box;
  }

  body {
    background: #111;
    color: #eee;
    text-align: center;
    font-family: Arial, sans-serif;
    margin: 0;
    padding: 20px;
  }

  h1 {
    margin: 12px 0 8px;
    font-size: 24px;
  }

  .subtitle {
    color: #999;
    font-size: 13px;
    margin-bottom: 24px;
  }

  .container {
    display: flex;
    flex-wrap: wrap;
    justify-content: center;
    align-items: flex-start;
    gap: 20px;
  }

  .panel {
    background: #1c1c1c;
    padding: 20px;
    border-radius: 12px;
    min-width: 285px;
  }

  .panel h2 {
    color: #ccc;
    font-size: 18px;
    margin: 0 0 15px;
  }

  canvas {
    width: min(400px, 85vw);
    height: auto;
    border: 2px solid #444;
    border-radius: 8px;
  }

  .legend-bar {
    width: 100%;
    height: 16px;
    border-radius: 5px;
    margin-top: 10px;
    background: linear-gradient(
      to right,
      blue, cyan, lime, yellow, red
    );
  }

  .legend-label {
    display: flex;
    justify-content: space-between;
    font-size: 12px;
    color: #bbb;
    margin-top: 5px;
  }

  .soil-value {
    color: #4fc3f7;
    font-size: 60px;
    font-weight: bold;
    margin-top: 18px;
  }

  .unit {
    color: #aaa;
    font-size: 13px;
  }

  .soil-status {
    margin: 20px auto 0;
    max-width: 300px;
    font-size: 15px;
    font-weight: bold;
    line-height: 1.5;
  }

  .moisture-bar {
    width: 260px;
    max-width: 100%;
    height: 23px;
    margin: 18px auto 8px;
    background: #333;
    border-radius: 14px;
    overflow: hidden;
  }

  .moisture-fill {
    width: 0%;
    height: 100%;
    background: linear-gradient(
      to right,
      #ff7043,
      #fdd835,
      #4caf50,
      #4fc3f7
    );
    transition: width 0.4s;
  }

  .scale-label {
    display: flex;
    justify-content: space-between;
    font-size: 12px;
    color: #aaa;
  }

  .stats {
    color: #bbb;
    margin-top: 14px;
    font-size: 14px;
    line-height: 1.6;
  }

  .note {
    color: #999;
    font-size: 12px;
    max-width: 310px;
    line-height: 1.6;
    margin: 18px auto 0;
  }

  .links {
    margin-top: 22px;
    font-size: 13px;
  }

  a {
    color: #80cbc4;
    margin: 0 10px;
  }
</style>
</head>

<body>

<h1>PlantCare: Thermal + Soil Monitor</h1>

<p class="subtitle">
Portulaca Plant Monitoring | ESP32 + AMG8833
</p>

<div class="container">

  <!-- Thermal Heatmap -->
  <section class="panel">
    <h2>Thermal Camera (AMG8833)</h2>

    <canvas id="thermal"
            width="400"
            height="400"></canvas>

    <div class="legend-bar"></div>

    <div class="legend-label">
      <span id="minLabel">--</span>
      <span id="maxLabel">--</span>
    </div>

    <div id="thermalStatus" class="stats">
      Waiting for thermal data...
    </div>

    <p class="note">
      Thermal data contains 8x8 measured pixels.
      The heatmap is interpolated for display.
    </p>
  </section>

  <!-- Soil Moisture -->
  <section class="panel">
    <h2>Soil Moisture</h2>

    <div class="soil-value" id="wetness">--%</div>

    <div class="unit">
      Relative Wetness Index (Experimental)
    </div>

    <div id="soilStatus" class="soil-status">
      Waiting for soil data...
    </div>

    <div class="moisture-bar">
      <div class="moisture-fill" id="moistureFill"></div>
    </div>

    <div class="scale-label">
      <span>0% Observed low</span>
      <span>100% Observed high</span>
    </div>

    <div id="soilDetails" class="stats">
      Waiting for ADC...
    </div>

    <p class="note">
      0% = average before watering (ADC 2614).
      100% = average immediately after watering
      (ADC 2274).

      These are experimental reference values,
      not true soil water percentages.
    </p>
  </section>

</div>

<div class="links">
  <a href="/soil">Soil JSON</a>
  <a href="/thermal">Thermal JSON</a>
  <a href="/status">Device Status</a>
</div>

<script>
//-- 19.1 Thermal Heatmap JavaScript
// ------------------------------------------------------------

// รับข้อมูลจาก /thermal เป็น Array จำนวน 64 ค่า
// จากนั้นขยายเป็นภาพ Heatmap ขนาด 64x64
const canvas = document.getElementById('thermal');
const ctx = canvas.getContext('2d');

const SIZE = 64;
const SRC = 8;

// แปลงอุณหภูมิเป็นสี
// น้ำเงิน -> Cyan -> Green -> Yellow -> Red
function tempToColor(ratio) {
  ratio = Math.max(0, Math.min(1, ratio));

  let r, g, b;

  if (ratio < 0.25) {
    r = 0;
    g = Math.floor(ratio * 4 * 255);
    b = 255;

  } else if (ratio < 0.5) {
    r = 0;
    g = 255;
    b = Math.floor(
      (1 - (ratio - 0.25) * 4) * 255
    );

  } else if (ratio < 0.75) {
    r = Math.floor(
      (ratio - 0.5) * 4 * 255
    );
    g = 255;
    b = 0;

  } else {
    r = 255;
    g = Math.floor(
      (1 - (ratio - 0.75) * 4) * 255
    );
    b = 0;
  }

  return [r, g, b];
}


// ขยายข้อมูล 8x8 ด้วย Bilinear Interpolation
function interpolate(data, x, y) {

  const fx = x * (SRC - 1) / (SIZE - 1);
  const fy = y * (SRC - 1) / (SIZE - 1);

  const x0 = Math.floor(fx);
  const y0 = Math.floor(fy);
  const x1 = Math.min(x0 + 1, SRC - 1);
  const y1 = Math.min(y0 + 1, SRC - 1);

  const dx = fx - x0;
  const dy = fy - y0;

  const v00 = data[y0 * SRC + x0];
  const v10 = data[y0 * SRC + x1];
  const v01 = data[y1 * SRC + x0];
  const v11 = data[y1 * SRC + x1];

  const v0 = v00 * (1 - dx) + v10 * dx;
  const v1 = v01 * (1 - dx) + v11 * dx;

  return v0 * (1 - dy) + v1 * dy;
}


// Canvas สำรองสำหรับสร้างภาพความร้อน
const tmpCanvas = document.createElement('canvas');

tmpCanvas.width = SIZE;
tmpCanvas.height = SIZE;

const tmpCtx = tmpCanvas.getContext('2d');


// เรียก /thermal เพื่ออัปเดต Heatmap
async function updateThermal() {

  try {
    const response = await fetch(
      '/thermal',
      { cache: 'no-store' }
    );

    if (!response.ok) {
      throw new Error('Thermal unavailable');
    }

    const data = await response.json();

    // ตรวจว่าข้อมูลมี 64 ค่า และเป็นตัวเลขทั้งหมด
    if (
      !Array.isArray(data) ||
      data.length !== 64 ||
      !data.every(Number.isFinite)
    ) {
      throw new Error('Invalid thermal data');
    }

    const actualMin = Math.min(...data);
    const actualMax = Math.max(...data);

    // ใช้ช่วงสีขั้นต่ำ 3 องศา
    // แต่แสดงค่า Min/Max จริงด้านล่างภาพ
    const displayMin = actualMin;
    const displayMax = Math.max(
      actualMax,
      actualMin + 3
    );

    const image = tmpCtx.createImageData(
      SIZE, SIZE
    );

    for (let y = 0; y < SIZE; y++) {
      for (let x = 0; x < SIZE; x++) {

        const temp = interpolate(data, x, y);

        const ratio =
          (temp - displayMin) /
          (displayMax - displayMin);

        const [r, g, b] = tempToColor(ratio);
        const index = (y * SIZE + x) * 4;

        image.data[index] = r;
        image.data[index + 1] = g;
        image.data[index + 2] = b;
        image.data[index + 3] = 255;
      }
    }

    tmpCtx.putImageData(image, 0, 0);

    ctx.imageSmoothingEnabled = true;

    ctx.drawImage(
      tmpCanvas, 0, 0,
      canvas.width, canvas.height
    );

    document.getElementById(
      'thermalStatus'
    ).textContent =
      'Minimum: ' + actualMin.toFixed(1) + ' C | ' +
      'Maximum: ' + actualMax.toFixed(1) + ' C';

    document.getElementById(
      'minLabel'
    ).textContent = displayMin.toFixed(1) + ' C';

    document.getElementById(
      'maxLabel'
    ).textContent = displayMax.toFixed(1) + ' C';

  } catch (error) {

    document.getElementById(
      'thermalStatus'
    ).textContent = 'Thermal unavailable';

    console.error(error);
  }
}


//-- 19.2 Soil Moisture JavaScript
// ------------------------------------------------------------

// รับ Soil JSON จาก /soil
// แล้วแสดงเปอร์เซ็นต์ทดลอง พร้อม ADC
async function updateSoil() {

  try {
    const response = await fetch(
      '/soil',
      { cache: 'no-store' }
    );

    if (!response.ok) {
      throw new Error('Soil unavailable');
    }

    const data = await response.json();

    // ถ้า Sensor ไม่พร้อม ห้ามแสดง 0% แทนค่าผิดพลาด
    if (
      !data.sensor_ok ||
      data.relative_wetness_pct === null
    ) {
      document.getElementById(
        'wetness'
      ).textContent = '--%';

      document.getElementById(
        'soilStatus'
      ).textContent = 'Sensor unavailable';

      document.getElementById(
        'moistureFill'
      ).style.width = '0%';

      return;
    }

    const pct = data.relative_wetness_pct;

    // แสดงตัวเลขเป็นเปอร์เซ็นต์
    document.getElementById(
      'wetness'
    ).textContent = pct.toFixed(1) + '%';

    // แสดงแถบสีตาม Relative Wetness Index
    document.getElementById(
      'moistureFill'
    ).style.width = pct + '%';

    // ใช้ข้อความ Soil Status จาก ESP32 โดยตรง
    document.getElementById(
      'soilStatus'
    ).textContent = data.status_text;

    // ยังเก็บค่า ADC ไว้เพื่อเทียบผลการทดลอง
    document.getElementById(
      'soilDetails'
    ).textContent =
      'ADC Average: ' + data.raw +
      ' | Filtered: ' + data.filtered_raw +
      ' | Calibration: Experimental';

  } catch (error) {

    document.getElementById(
      'soilStatus'
    ).textContent = 'Soil unavailable';

    console.error(error);
  }
}


//-- 19.3 Web Refresh Loop
// ------------------------------------------------------------

// ดึงข้อมูลทันทีเมื่อเปิดหน้าเว็บ
updateThermal();
updateSoil();

// อัปเดตทุก 2 วินาที
setInterval(updateThermal, 2000);
setInterval(updateSoil, 2000);

</script>
</body>
</html>
)HTML";


//-- 20. HTTP Root Handler
// ------------------------------------------------------------

// ส่งหน้า Web Dashboard ให้ Browser เมื่อเปิด /
void handleRoot() {

  server.sendHeader("Cache-Control", "no-store");

  server.send_P(
    200,
    "text/html; charset=utf-8",
    index_html
  );
}


//-- 21. HTTP Thermal JSON
// ------------------------------------------------------------

// HTTP GET /thermal
// ส่งค่าจาก AMG8833 จำนวน 64 Pixels
//
// ข้อมูลนี้ถูกนำไปใช้สร้าง Heatmap บน Web
// และสามารถให้ Node-RED อ่านได้ในอนาคต
void handleThermal() {

  if (!thermalReady) {
    server.send(
      503,
      "application/json",
      "{\"error\":\"thermal_unavailable\"}"
    );
    return;
  }

  String json = "[";
  json.reserve(500);

  for (int i = 0; i < 64; i++) {

    if (i > 0) {
      json += ",";
    }

    json += String(thermalPixels[i], 2);
  }

  json += "]";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}


//-- 22. HTTP Soil JSON
// ------------------------------------------------------------

// HTTP GET /soil
// ส่งข้อมูล Soil สำหรับ Web Dashboard
//
// Fields:
// raw                   ค่า ADC เฉลี่ย
// filtered_raw          ADC หลัง EMA
// relative_wetness_pct  ดัชนีความชื้นทดลอง (%)
// status_text           สถานะเป็นภาษาอังกฤษ
// sensor_ok             สถานะการอ่าน Soil
// calibration_method    วิธีสร้างค่าอ้างอิง
// data_source           ข้อมูลจากเซนเซอร์จริง
//
// ไม่ใช้ชื่อ moisture_pct เพราะอาจถูกเข้าใจผิดว่าเป็น
// เปอร์เซ็นต์ความชื้นทางกายภาพที่ผ่าน Calibration แล้ว
void handleSoil() {

  String json = "{";
  json.reserve(450);

  json += "\"raw\":";
  json += String(soilRaw);
  json += ",";

  json += "\"filtered_raw\":";

  if (soilReady && soilFilterReady) {
    json += String(soilFiltered, 1);
  } else {
    json += "null";
  }

  json += ",\"relative_wetness_pct\":";

  if (soilReady && soilFilterReady) {
    json += String(relativeWetnessPct, 1);
  } else {
    json += "null";
  }

  json += ",\"status_text\":\"";
  json += getSoilStatus();
  json += "\",";

  json += "\"sensor_ok\":";
  json += (soilReady ? "true" : "false");

  json += ",\"reference_before_water_adc\":";
  json += String(SOIL_BEFORE_WATER_REF, 0);

  json += ",\"reference_after_water_adc\":";
  json += String(SOIL_AFTER_WATER_REF, 0);

  json += ",\"calibration_method\":";
  json += "\"observed_before_after_mean\"";

  json += ",\"calibrated_absolute_moisture\":false";

  json += ",\"data_source\":\"real\"";

  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}


//-- 23. HTTP Device Status JSON
// ------------------------------------------------------------

// HTTP GET /status
// แสดงว่า ESP32 และแต่ละ Sensor พร้อมหรือไม่
//
// ข้อมูลนี้เป็นพื้นฐานสำหรับการทำ Monitoring
// ผ่าน Node-RED หรือระบบอื่นในอนาคต
void handleStatus() {

  String json = "{";

  json += "\"device_id\":\"esp32_01\",";
  json += "\"device_status\":\"";

  json += (
    WiFi.status() == WL_CONNECTED
      ? "online"
      : "degraded"
  );

  json += "\",";

  json += "\"soil_sensor_ok\":";
  json += (soilReady ? "true" : "false");
  json += ",";

  json += "\"thermal_sensor_ok\":";
  json += (thermalReady ? "true" : "false");
  json += ",";

  json += "\"lcd_available\":";
  json += (lcd != nullptr ? "true" : "false");
  json += ",";

  json += "\"uptime_ms\":";
  json += String(millis());
  json += ",";

  json += "\"data_source\":\"real\"";

  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}


//-- 24. Wi-Fi Connection
// ------------------------------------------------------------

// เชื่อมต่อ Wi-Fi ผ่านข้อมูลในไฟล์เดียวกัน
//
// ถ้า Wi-Fi ใช้งานไม่ได้:
// - LCD ยังแสดงผลได้
// - Serial Monitor ยังแสดงผลได้
// - Sensor ยังอ่านค่าได้
// - แต่ Web Dashboard จะยังเข้าถึงไม่ได้
void connectWiFi() {

  WiFi.mode(WIFI_STA);

  // ให้ ESP32 ลองเชื่อมต่อใหม่เมื่อ Wi-Fi หลุด
  WiFi.setAutoReconnect(true);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WIFI] Connecting");

  unsigned long startedAt = millis();

  // รอเชื่อมต่อไม่เกิน 20 วินาที
  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startedAt < 20000
  ) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("[WIFI] Connected");

    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());

  } else {

    Serial.println("[WIFI] Connection failed");
    Serial.println("[WIFI] Check SSID and 2.4 GHz network");
  }
}


//-- 25. Start HTTP Web Server
// ------------------------------------------------------------

// ลงทะเบียน HTTP URL สำหรับหน้าเว็บและ JSON
//
// เริ่ม Web Server เพียงครั้งเดียว
// เมื่อ Wi-Fi เชื่อมต่อสำเร็จ
void startWebServer() {

  if (httpStarted) {
    return;
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/soil", HTTP_GET, handleSoil);
  server.on("/thermal", HTTP_GET, handleThermal);
  server.on("/status", HTTP_GET, handleStatus);

  server.begin();
  httpStarted = true;

  Serial.println("[HTTP] Server started");

  Serial.print("[HTTP] Dashboard: http://");
  Serial.println(WiFi.localIP());
}


//-- 25.1 Cloud HTTPS Uploader (Node-RED -> InfluxDB)
// ------------------------------------------------------------
// Root CA from Let's Encrypt: ISRG Root X2 (self-signed).
// SHA-256 fingerprint: 69729B8E15A86EFC177A57AFB7171DFC64ADD28C2FCA8CF1507E34453CCB1470
// The HTTPS server must present a chain ending in this root.
// A secure clock (NTP) is needed to check certificate dates.


void beginCloudUploader() {
  lastCloudAttemptAt = millis();
  // Set system time in UTC; server generates the database timestamp.
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.println("[CLOUD] HTTPS uploader initialized (60-second interval)");
}

// Uses the last values read in sampleSensors(): no second sensor read.
String buildCloudPayload() {
  String json;
  json.reserve(1400);
  json = "{\"plant_id\":\"plant01\",\"device_id\":\"esp32_01\",\"data_source\":\"real\"";

  if (soilReady && soilFilterReady && isfinite(soilFiltered) && isfinite(relativeWetnessPct)) {
    json += ",\"soil\":{\"sensor_ok\":true,\"raw\":";
    json += String(soilRaw);
    json += ",\"filtered_raw\":";
    json += String(soilFiltered, 2);
    json += ",\"relative_wetness_pct\":";
    json += String(relativeWetnessPct, 2);
    json += "}";
  }

  if (thermalReady) {
    json += ",\"thermal\":{\"pixels\":[";
    for (int i = 0; i < 64; i++) {
      if (i) json += ',';
      json += String(thermalPixels[i], 2);
    }
    json += "]}";
  }
  json += "}";
  return json;
}

void uploadCloudIfDue() {
  if (WiFi.status() != WL_CONNECTED) return;
  const unsigned long now = millis();
  if (now - lastCloudAttemptAt < CLOUD_UPLOAD_INTERVAL_MS) return;
  lastCloudAttemptAt = now;

  // Do not transmit if configuration has not been filled in.
  if (strlen(CLOUD_API_KEY) < 32 ||
      strcmp(CLOUD_API_KEY, "PASTE_YOUR_32_CHARACTER_API_KEY_HERE") == 0) {
    Serial.println("[CLOUD] Configure CLOUD_API_KEY first");
    return;
  }

  // TLS checks certificate dates, so do not try while clock is 1970.
  if (time(nullptr) < 1735689600) { // 2025-01-01 UTC
    Serial.println("[CLOUD] Waiting for NTP clock synchronization");
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    return;
  }

  if (!(soilReady && soilFilterReady) && !thermalReady) {
    Serial.println("[CLOUD] Sensor data not ready; skip upload");
    return;
  }

  WiFiClientSecure client;
  client.setCACert(CLOUD_ROOT_CA); // Never use setInsecure().

  HTTPClient https;
  https.setConnectTimeout(4000);
  https.setTimeout(5000);
  https.setReuse(false);

  if (!https.begin(client, CLOUD_URL)) {
    Serial.println("[CLOUD] Failed to initialize HTTPS");
    return;
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("x-plantcare-key", CLOUD_API_KEY);

  const String payload = buildCloudPayload();
  const int status = https.POST(payload);

  if (status == 202) {
    // Node-RED accepted the message. DB persistence is checked separately.
    Serial.println("[CLOUD] Node-RED accepted JSON (HTTP 202)");
  } else if (status > 0) {
    Serial.printf("[CLOUD] HTTP status: %d\n", status);
    Serial.println(https.getString());
  } else {
    Serial.printf("[CLOUD] HTTPS failed: %s\n", https.errorToString(status).c_str());
  }
  https.end();
}

//-- 26. Arduino setup()
// ------------------------------------------------------------

// setup() ทำงานหนึ่งครั้งเมื่อเปิดเครื่องหรือกด Reset
//
// ลำดับการเริ่มต้น:
// 1. เปิด Serial Monitor
// 2. ตั้งค่า Soil ADC
// 3. เริ่ม I2C Bus
// 4. เริ่ม AMG8833
// 5. เริ่ม LCD
// 6. อ่านเซนเซอร์ครั้งแรก
// 7. เชื่อม Wi-Fi
// 8. เปิด Web Server
void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("==================================");
  Serial.println("PlantCare IoT - Thermal and Soil");
  Serial.println("==================================");

  // ตั้ง ADC ให้มีความละเอียด 12-bit
  // ค่าที่อ่านได้อยู่ในช่วง 0-4095
  analogReadResolution(12);

  // กำหนด Attenuation ของ GPIO34
  // เพื่ออ่านสัญญาณ Analog จาก Soil Sensor
  analogSetPinAttenuation(
    SOIL_PIN,
    ADC_11db
  );

  pinMode(SOIL_PIN, INPUT);

  Serial.println("[SOIL] GPIO34 configured");

  // เริ่ม I2C ของ AMG8833 และ LCD
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);

  // ดู Address ของอุปกรณ์ที่เชื่อมอยู่
  scanI2CDevices();

  // เริ่ม Thermal Sensor
  amgConnected = initializeAMG8833();

  // เริ่ม LCD ก่อนเปิด Wi-Fi
  // เพื่อให้เห็นข้อความแม้ Wi-Fi ไม่พร้อม
  initializeLCD();

  // อ่านค่าครั้งแรก
  sampleSensors();

  // ส่งผลครั้งแรกออก LCD และ Serial
  updateLCD();
  printSerialData();

  lastSampleAt = millis();
  lastDisplayAt = millis();
  lastAmgRetryAt = millis();

  // เชื่อมต่อเครือข่าย Wi-Fi
  connectWiFi();

  // เปิด Web Server หลัง Wi-Fi พร้อม
  if (WiFi.status() == WL_CONNECTED) {
    startWebServer();
  }

  beginCloudUploader();
}


//-- 27. Arduino loop()
// ------------------------------------------------------------

// loop() ทำงานซ้ำตลอดเวลาที่ ESP32 เปิดอยู่
//
// ใช้ millis() จัดเวลาแยกงานต่าง ๆ
// ทำให้:
// - อ่านเซนเซอร์
// - อัปเดต LCD
// - แสดง Serial
// - รับ HTTP Request
// สามารถทำงานร่วมกันในโปรแกรมเดียว
void loop() {

  unsigned long now = millis();

  // อ่าน Soil และ Thermal ทุก 2 วินาที
  if (
    now - lastSampleAt >= SAMPLE_INTERVAL_MS
  ) {
    lastSampleAt = now;
    sampleSensors();
  }

  // แสดงข้อมูลบน LCD และ Serial ทุก 2 วินาที
  if (
    now - lastDisplayAt >= DISPLAY_INTERVAL_MS
  ) {
    lastDisplayAt = now;

    updateLCD();
    printSerialData();
  }

  // หาก AMG8833 ไม่พร้อม
  // ลองเชื่อมต่อใหม่ทุก 10 วินาที
  if (
    !amgConnected &&
    now - lastAmgRetryAt >= RETRY_INTERVAL_MS
  ) {
    lastAmgRetryAt = now;

    amgConnected = initializeAMG8833();
  }

  // หาก Wi-Fi หลุด
  // ลองเชื่อมต่อใหม่ทุก 10 วินาที
  if (
    WiFi.status() != WL_CONNECTED &&
    now - lastWiFiRetryAt >= RETRY_INTERVAL_MS
  ) {
    lastWiFiRetryAt = now;

    Serial.println("[WIFI] Reconnecting...");
    WiFi.reconnect();
  }

  // หาก Wi-Fi พร้อมแต่ Web Server ยังไม่เริ่ม
  // ให้เริ่ม Server โดยไม่ต้องกด Reset
  if (
    WiFi.status() == WL_CONNECTED &&
    !httpStarted
  ) {
    startWebServer();
  }

  // รับ HTTP Request จากหน้า Browser
  if (httpStarted) {
    server.handleClient();
  }

  // ส่ง Sensor JSON ไป Node-RED ทุก 60 วินาที
  uploadCloudIfDue();

  // พักระยะสั้น เพื่อให้ระบบ Task อื่นทำงานได้
  delay(2);
}
