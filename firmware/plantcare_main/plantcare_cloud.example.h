// ไฟล์ตัวอย่างจาก header เดิม: sketch หลักไม่ได้ include และมี uploader ของตัวเองอยู่แล้ว

#pragma once

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include <math.h>

// PlantCare Cloud Configuration
// IMPORTANT: Keep this file private. Do not commit API keys.

static const char CLOUD_URL[] = "https://example.invalid/plantcare/ingest";

static const char CLOUD_API_KEY[] = "";

static const unsigned long CLOUD_INTERVAL_MS = 60000UL;
static unsigned long cloudLastAttemptMs = 0;

// Let's Encrypt ISRG Root X2 - self-signed root certificate
static const char CLOUD_ROOT_CA[] = R"PEM(
-----BEGIN CERTIFICATE-----
MIICGzCCAaGgAwIBAgIQQdKd0XLq7qeAwSxs6S+HUjAKBggqhkjOPQQDAzBPMQsw
CQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2gg
R3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMjAeFw0yMDA5MDQwMDAwMDBaFw00
MDA5MTcxNjAwMDBaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5ldCBT
ZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgyMHYw
EAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0HttwW
+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7AlF9
ItgKbppbd9/w+kHsOdx1ymgHDB/qo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0T
AQH/BAUwAwEB/zAdBgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwCgYIKoZI
zj0EAwMDaAAwZQIwe3lORlCEwkSHRhtFcP9Ymd70/aTSVaYgLXTWNLxBo1BfASdW
tL4ndQavEi51mI38AjEAi/V3bNTIZargCyzuFJ0nN6T5U6VR5CmD1/iQMVtCnwr1
/q4AaOeMSQ+2b1tbFfLn
-----END CERTIFICATE-----
)PEM";

// Initialize NTP time synchronization for TLS certificate checks.
void cloudBegin() {
  cloudLastAttemptMs = millis();

  configTime(
    0,
    0,
    "pool.ntp.org",
    "time.google.com"
  );

  Serial.println("[CLOUD] HTTPS uploader initialized");
}

// Build JSON from the existing sensor variables.
// Does not read sensors again.
String cloudBuildPayload() {
  String json;
  json.reserve(1100);

  json = "{";
  json += "\"plant_id\":\"plant01\",";
  json += "\"device_id\":\"esp32_01\",";
  json += "\"data_source\":\"real\"";

  if (soilReady && soilFilterReady) {
    json += ",\"soil\":{";
    json += "\"sensor_ok\":true,";
    json += "\"raw\":";
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
      if (i > 0) {
        json += ",";
      }

      json += String(thermalPixels[i], 2);
    }

    json += "]}";
  }

  json += "}";
  return json;
}

// Called regularly by the existing Arduino loop().
void cloudTick() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long nowMs = millis();

  if (nowMs - cloudLastAttemptMs < CLOUD_INTERVAL_MS) {
    return;
  }

  cloudLastAttemptMs = nowMs;

  if (strlen(CLOUD_API_KEY) < 32) {
    Serial.println("[CLOUD] API key is not configured");
    return;
  }

  // Ensure clock has synchronized before TLS verification.
  time_t currentTime = time(nullptr);

  if (currentTime < 1735689600) {
    Serial.println("[CLOUD] Waiting for NTP time sync");
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    return;
  }

  if (
    !(soilReady && soilFilterReady) &&
    !thermalReady
  ) {
    Serial.println("[CLOUD] No valid sensor data");
    return;
  }

  String payload = cloudBuildPayload();

  WiFiClientSecure secureClient;
  secureClient.setCACert(CLOUD_ROOT_CA);

  HTTPClient https;
  https.setConnectTimeout(4000);
  https.setTimeout(5000);
  https.setReuse(false);

  if (!https.begin(secureClient, CLOUD_URL)) {
    Serial.println("[CLOUD] HTTPS initialization failed");
    return;
  }

  https.addHeader(
    "Content-Type",
    "application/json"
  );

  https.addHeader(
    "x-plantcare-key",
    CLOUD_API_KEY
  );

  int statusCode = https.POST(payload);

  if (statusCode == 202) {
    Serial.println("[CLOUD] Accepted by Node-RED (202)");
  } else if (statusCode > 0) {
    Serial.printf(
      "[CLOUD] HTTP status: %d\n",
      statusCode
    );
    Serial.println(https.getString());
  } else {
    Serial.printf(
      "[CLOUD] Request failed: %s\n",
      https.errorToString(statusCode).c_str()
    );
  }

  https.end();
}
