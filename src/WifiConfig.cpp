#include "WifiConfig.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <bearssl/bearssl.h>
#include <ConfigKey.h>

namespace {
template<class Config>
struct Record {
    uint8_t nonce[12];
    uint8_t data[sizeof(Config)];
    uint8_t tag[16];
};

template<class Config>
void crypt(const char* path, Record<Config>& record, uint8_t* tag, bool encrypt) {
    br_poly1305_ctmul_run(configKey, record.nonce, record.data, sizeof(record.data),
                          path, strlen(path) + 1, tag, br_chacha20_ct_run, encrypt);
}

template<class Config>
bool readConfig(const char* path, const char* name, Config& config) {
    File file = LittleFS.open(name, "r");
    Record<Config> record;
    if (!file || file.size() != sizeof(record) ||
        file.read(reinterpret_cast<uint8_t*>(&record), sizeof(record)) != sizeof(record)) {
        return false;
    }
    uint8_t expected[16];
    crypt(path, record, expected, false);
    unsigned difference = 0;
    for (size_t i = 0; i < sizeof(expected); ++i) {
        difference |= expected[i] ^ record.tag[i];
    }
    if (difference) return false;
    memcpy(&config, record.data, sizeof(config));
    return true;
}

template<class Config>
bool saveConfig(const char* path, const char* temporary, const Config& config) {
    Record<Config> record;
    ESP.random(record.nonce, sizeof(record.nonce));
    memcpy(record.data, &config, sizeof(config));
    crypt(path, record, record.tag, true);
    File file = LittleFS.open(temporary, "w");
    if (!file || file.write(reinterpret_cast<const uint8_t*>(&record), sizeof(record)) != sizeof(record)) {
        return false;
    }
    file.close();
    Config check;
    return readConfig(path, temporary, check) && !memcmp(&check, &config, sizeof(config))
        && LittleFS.rename(temporary, path);
}
}

bool loadWifiConfig(WifiConfig& config) {
    return readConfig("/wifi.v1", "/wifi.v1", config);
}

bool saveWifiConfig(const WifiConfig& config) {
    return saveConfig("/wifi.v1", "/wifi.tmp", config);
}

bool loadMqttConfig(MqttConfig& config) {
    return readConfig("/mqtt.v1", "/mqtt.v1", config);
}

bool saveMqttConfig(const MqttConfig& config) {
    return saveConfig("/mqtt.v1", "/mqtt.tmp", config);
}
