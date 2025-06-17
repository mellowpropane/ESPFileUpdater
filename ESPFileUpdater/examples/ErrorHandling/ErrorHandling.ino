#include <Arduino.h>
#include <WiFi.h>           // ESP32/ESP8266 WiFi
#include <SPIFFS.h>         // or #include <LittleFS.h> for LittleFS
#include "espfileupdater.h" // Your ESPFileUpdater class

// WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Helper for status
bool wasUpdated(ESPFileUpdater::UpdateStatus status) {
    return status == ESPFileUpdater::UPDATED;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize SPIFFS (or LittleFS)
    if (!SPIFFS.begin(true)) { // true = format if failed
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    // For LittleFS, use: if (!LittleFS.begin()) { ... }

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");

    // (Not Optional) Set time via NTP for maxAge support
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = time(nullptr);
    while (now < 100000) {
        delay(100);
        now = time(nullptr);
    }
    Serial.println("Time synchronized!");

    // Create updater instance
    ESPFileUpdater updater(SPIFFS); // or LittleFS

    // Casts param to updater pointer
    ESPFileUpdater* updater = (ESPFileUpdater*)param;

    // Run update
    ESPFileUpdater::UpdateStatus result = updater->checkAndUpdate(
      "/www/timezones.json.gz", // local file
      "https://raw.githubusercontent.com/trip5/timezones.json/master/timezones.min.json.gz", //remote file
      "7d", // how often to check if an update is valid to attempt
      true // verbose logging
  );

    // --- Detailed error handling ---
    if (wasUpdated(result)) {
        Serial.println("[Main] File was updated successfully.");
    } else {
        switch (result) {
            case ESPFileUpdater::NOT_MODIFIED:
                Serial.println("File is already up-to-date.");
                break;
            case ESPFileUpdater::MAX_AGE_NOT_REACHED:
                Serial.println("Max age not reached; skipping update.");
                break;
            case ESPFileUpdater::FILE_NOT_FOUND:
                Serial.println("Remote file not found (404).");
                break;
            case ESPFileUpdater::SERVER_ERROR:
                Serial.println("Server error or unreachable.");
                break;
            case ESPFileUpdater::SPIFFS_ERROR:
                Serial.println("SPIFFS error during file update.");
                break;
            case ESPFileUpdater::TIME_ERROR:
                Serial.println("System time not set. Update aborted.");
                break;
}
            default:
                Serial.println("Unknown update status.");
        }
    }
}

void loop() {
    // Your main code here
}