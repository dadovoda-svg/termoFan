#pragma once

struct WifiConfig {
    char ssid[33] = {};
    char password[65] = {};
    char adminPassword[65] = "default";
};

bool loadWifiConfig(WifiConfig& config);
bool saveWifiConfig(const WifiConfig& config);

struct MqttConfig {
    char host[254] = {};
    char username[65] = {};
    char password[129] = {};
    unsigned short port = 1883;
};

bool loadMqttConfig(MqttConfig& config);
bool saveMqttConfig(const MqttConfig& config);
