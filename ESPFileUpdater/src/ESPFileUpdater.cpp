#include "ESPFileUpdater.h"
#include <time.h>

ESPFileUpdater::ESPFileUpdater(fs::FS& fs) : _fs(fs) {}

ESPFileUpdater::UpdateStatus ESPFileUpdater::checkAndUpdate(const String& localPath, const String& remoteURL, const String& maxAge) {
    String meta = metaPath(localPath);
    if (maxAge.length() > 0 && _fs.exists(meta)) {
        time_t metaTime = parseMetaTime(meta);
        time_t now = time(nullptr);
        time_t interval = parseMaxAge(maxAge);
        if (metaTime > 0 && interval > 0 && (now < metaTime + interval)) {
            Serial.printf("[ESPFileUpdater] [Info] Skipping update: maxAge (%s) not reached.\n", maxAge.c_str());
            return MAX_AGE_NOT_REACHED;
        }

        char metaTimeStr[32];
        char nowStr[32];
        strftime(metaTimeStr, sizeof(metaTimeStr), "%Y-%m-%d %H:%M:%S", localtime(&metaTime));
        strftime(nowStr, sizeof(nowStr), "%Y-%m-%d %H:%M:%S", localtime(&now));

        Serial.printf("[ESPFileUpdater] Current time: %s (epoch: %ld)\n", nowStr, now);
        Serial.printf("[ESPFileUpdater] .meta file time: %s (epoch: %ld)\n", metaTimeStr, metaTime);
    }

    Serial.printf("[ESPFileUpdater: %s] [Check] Connecting to %s\n", localPath.c_str(), remoteURL.c_str());

    HTTPClient http;
    http.begin(remoteURL);
    int httpCode = http.sendRequest("HEAD");

    if (httpCode <= 0) {
        Serial.println("[ESPFileUpdater] [Error] Server unreachable.");
        return SERVER_ERROR;
    }

    Serial.printf("[ESPFileUpdater: %s] [Response] HTTP code: %d\n", localPath.c_str(), httpCode);
    if (httpCode == HTTP_CODE_NOT_FOUND) {
        Serial.println("[ESPFileUpdater] [Error] File not found on server.");
        return FILE_NOT_FOUND;
    }

    String lastModified = http.header("Last-Modified");
    String contentLengthStr = http.header("Content-Length");

    Serial.printf("[ESPFileUpdater: %s] [Header] Last-Modified: %s\n", localPath.c_str(), lastModified.c_str());
    Serial.printf("[ESPFileUpdater: %s] [Header] Content-Length: %s\n", localPath.c_str(), contentLengthStr.c_str());

    String newLastModified;
    String remoteHash;
    UpdateStatus status = isRemoteFileNewer(localPath, remoteURL, lastModified, newLastModified, remoteHash);

    if (status != UPDATED) {
        if (status == NOT_MODIFIED)
            Serial.printf("[ESPFileUpdater: %s] [Check] Remote file not modified.\n", localPath.c_str());
        return status;
    }

    Serial.printf("[ESPFileUpdater: %s] [Update] Remote file is newer. Downloading...\n", localPath.c_str());

    http.end();

    http.begin(remoteURL);
    int getCode = http.GET();
    if (getCode != HTTP_CODE_OK) {
        Serial.printf("[ESPFileUpdater] [Error] GET failed. HTTP code: %d\n", getCode);
        http.end();
        return SERVER_ERROR;
    }

    ensureDirExists(localPath);

    String tmpPath = localPath + ".tmp";
    File file = _fs.open(tmpPath, FILE_WRITE);
    if (!file) {
        Serial.println("[ESPFileUpdater] [Error] Cannot open temp file for writing.");
        http.end();
        return SPIFFS_ERROR;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[512];
    int len;
    while ((len = stream->readBytes(buf, sizeof(buf))) > 0) {
        file.write(buf, len);
    }

    file.close();
    http.end();

    _fs.remove(localPath);
    _fs.rename(tmpPath, localPath);
    writeMeta(metaPath(localPath), newLastModified, remoteHash);
    return UPDATED;
}

ESPFileUpdater::UpdateStatus ESPFileUpdater::isRemoteFileNewer(const String& localPath, const String& url,
                                                               const String& lastModified,
                                                               String& newLastModified,
                                                               String& remoteHash) {
    String meta = metaPath(localPath);
    String storedLastMod = readMetaLastModified(meta);
    String storedHash = readMetaHash(meta);

    if (!_fs.exists(localPath)) {
        Serial.println("[ESPFileUpdater] [Decision] File doesn't exist. Will update.");
        return UPDATED;
    }

    if (lastModified.length() == 0) {
        Serial.printf("[ESPFileUpdater: %s] [Fallback] No Last-Modified header. Comparing file hashes...\n", localPath.c_str());

        HTTPClient http;
        http.begin(url);
        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            http.end();
            return SERVER_ERROR;
        }

        remoteHash = calculateStreamHash(*http.getStreamPtr(), ESPFILEUPDATER_MAXSIZE);
        http.end();

        File localFile = _fs.open(localPath, FILE_READ);
        String localHash = calculateFileHash(localFile);
        localFile.close();

        Serial.printf("[ESPFileUpdater] [Local SHA256] %s\n", localHash.c_str());
        Serial.printf("[ESPFileUpdater] [Remote SHA256] %s\n", remoteHash.c_str());

        if (localHash != remoteHash) {
            return UPDATED;
        } else {
            time_t now = time(nullptr);
            String nowStr = String((uint32_t)now);
            writeMeta(metaPath(localPath), nowStr, localHash);
            Serial.println("[ESPFileUpdater] Hashes match, updated .meta file date to now.");
            return NOT_MODIFIED;
        }
    }

    if (lastModified != storedLastMod) {
        newLastModified = lastModified;
        return UPDATED;
    }

    return NOT_MODIFIED;
}

String ESPFileUpdater::metaPath(const String& filePath) {
    return filePath + ".meta";
}

String ESPFileUpdater::readMetaLastModified(const String& metaPath) {
    File meta = _fs.open(metaPath, FILE_READ);
    if (!meta) return "";
    String line = meta.readStringUntil('\n');
    meta.close();
    return line;
}

String ESPFileUpdater::readMetaHash(const String& metaPath) {
    File meta = _fs.open(metaPath, FILE_READ);
    if (!meta) return "";
    meta.readStringUntil('\n');
    String line = meta.readStringUntil('\n');
    meta.close();
    return line;
}

bool ESPFileUpdater::writeMeta(const String& metaPath, const String& lastModified, const String& sha256) {
    File meta = _fs.open(metaPath, FILE_WRITE);
    if (!meta) return false;
    meta.println(lastModified);
    meta.println(sha256);
    meta.close();
    return true;
}

String ESPFileUpdater::calculateFileHash(File& file) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);

    uint8_t buffer[512];
    int len;
    while ((len = file.read(buffer, sizeof(buffer))) > 0) {
        mbedtls_sha256_update_ret(&ctx, buffer, len);
    }

    uint8_t hash[32];
    mbedtls_sha256_finish_ret(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    String result;
    for (int i = 0; i < 32; i++) {
        if (hash[i] < 16) result += "0";
        result += String(hash[i], HEX);
    }
    return result;
}

String ESPFileUpdater::calculateStreamHash(WiFiClient& stream, size_t maxBytes) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts_ret(&ctx, 0);

    uint8_t buffer[512];
    size_t total = 0;
    int len;

    while (total < maxBytes && stream.connected() &&
           (len = stream.readBytes(buffer, sizeof(buffer))) > 0) {
        mbedtls_sha256_update_ret(&ctx, buffer, len);
        total += len;
    }

    uint8_t hash[32];
    mbedtls_sha256_finish_ret(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    String result;
    for (int i = 0; i < 32; i++) {
        if (hash[i] < 16) result += "0";
        result += String(hash[i], HEX);
    }
    return result;
}

bool ESPFileUpdater::ensureDirExists(const String& path) {
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash <= 0) return true;

    String dir = path.substring(0, lastSlash);
    if (!_fs.exists(dir)) {
        return _fs.mkdir(dir);
    }
    return true;
}

time_t ESPFileUpdater::parseMaxAge(const String& maxAgeStr) {
    String s = maxAgeStr;
    s.toLowerCase();
    s.trim();
    s.replace(" ", "");
    int idx = 0;
    while (idx < s.length() && isDigit(s[idx])) idx++;
    if (idx == 0) return 0;

    int num = s.substring(0, idx).toInt();
    String unit = s.substring(idx);
    unit.replace("s", "");

    if (unit == "hour" || unit == "hr" || unit == "h")
        return num * 3600;
    if (unit == "day" || unit == "d")
        return num * 86400;
    if (unit == "month" || unit == "mo" || unit == "m")
        return num * 2592000;

    return 0;
}

time_t ESPFileUpdater::parseMetaTime(const String& metaPath) {
    File meta = _fs.open(metaPath, FILE_READ);
    if (!meta) return 0;
    String line = meta.readStringUntil('\n');
    meta.close();
    line.trim();
    if (line.length() == 0) return 0;
    if (line.toInt() > 100000) return line.toInt();
    struct tm t;
    if (strptime(line.c_str(), "%a, %d %b %Y %H:%M:%S", &t)) {
        return mktime(&t);
    }
    return 0;
}