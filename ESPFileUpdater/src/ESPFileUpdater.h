#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <FS.h>
#include <mbedtls/sha256.h>

#ifndef ESPFILEUPDATER_MAXSIZE
#define ESPFILEUPDATER_MAXSIZE 102400  // 100 KB max stream size for hashing
#endif

class ESPFileUpdater {
public:
    enum UpdateStatus {
        UPDATED,
        MAX_AGE_NOT_REACHED,
        NOT_MODIFIED,
        SERVER_ERROR,
        FILE_NOT_FOUND,
        SPIFFS_ERROR
    };

    ESPFileUpdater(fs::FS& fs);

    UpdateStatus checkAndUpdate(const String& localPath, const String& remoteURL, const String& maxAge = "");

private:
    fs::FS& _fs;

    String metaPath(const String& filePath);
    String readMetaLastModified(const String& metaPath);
    String readMetaHash(const String& metaPath);
    bool writeMeta(const String& metaPath, const String& lastModified, const String& sha256);

    String calculateFileHash(File& file);
    String calculateStreamHash(WiFiClient& stream, size_t maxBytes);
    bool ensureDirExists(const String& path);
    bool fileExists(const String& path);
    time_t parseMaxAge(const String& maxAgeStr);
    time_t parseMetaTime(const String& metaPath);

    UpdateStatus isRemoteFileNewer(const String& localPath, const String& url,
                                    const String& lastModified, String& newLastModified, String& remoteHash);
};