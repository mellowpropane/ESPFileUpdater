#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include "ESPFileUpdater.h"

// WiFi credentials
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected!");

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


    // Handle update result (optional)
  if (result == ESPFileUpdater::UPDATED) {
    Serial.println("[ESPFileUpdater: File.name.ext] Update completed.");
  } else if (result == ESPFileUpdater::NOT_MODIFIED) {
    Serial.println("[ESPFileUpdater: File.name.ext] No update needed.");
  } else {
    Serial.println("[ESPFileUpdater: File.name.ext] Update failed.");
  }
}

void loop() {
    // Your main code here
}