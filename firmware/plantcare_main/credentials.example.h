#pragma once

// Copy this file to credentials.h and configure it locally.
static constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
static constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
static constexpr char CLOUD_API_KEY[] = "PASTE_YOUR_32_CHARACTER_API_KEY_HERE";
static constexpr char CLOUD_URL[] = "https://plantcare.example.invalid/plantcare/ingest";

// Paste the PEM CA certificate that validates your HTTPS endpoint.
// Keep the private key on the server. A CA certificate is sufficient here.
static constexpr char CLOUD_ROOT_CA[] = R"PEM(
)PEM";
