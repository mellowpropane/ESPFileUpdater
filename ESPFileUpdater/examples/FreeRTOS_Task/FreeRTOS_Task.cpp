#include "../ESPFileUpdater/ESPFileUpdater.h"

// This example from network.cpp in my PR to yoRadio
// https://github.com/e2002/yoradio/pull/184

bool wasUpdated(ESPFileUpdater::UpdateStatus status) { return status == ESPFileUpdater::UPDATED; }
ESPFileUpdater updater(SPIFFS);

// 

void updateTZjson(void* param) {
  Serial.println("[ESPFileUpdater: Timezones.json] Called by TimeSync");
  ESPFileUpdater* updater = (ESPFileUpdater*)param;
  ESPFileUpdater::UpdateStatus result = updater->checkAndUpdate(
      "/www/timezones.json.gz",
      TIMEZONES_JSON_GZ_URL,
      "1 week", // update once a week at most
      false // verbose logging
  );
  if (result == ESPFileUpdater::UPDATED) {
    Serial.println("[ESPFileUpdater: Timezones.json] Update completed.");
  } else if (result == ESPFileUpdater::NOT_MODIFIED) {
    Serial.println("[ESPFileUpdater: Timezones.json] No update needed.");
  } else {
    Serial.println("[ESPFileUpdater: Timezones.json] Update failed.");
  }
  vTaskDelete(NULL);
}

