#include "Mqtt.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

namespace {
WiFiClient transport;
PubSubClient client(transport);
MqttConfig broker;
unsigned long lastAttempt;
unsigned long lastState;
bool attempted = false;
bool discoveryPending = false;
char deviceId[24];
char availabilityTopic[64];
char stateTopic[64];

bool publishDiscovery() {
    char topic[96];
    char payload[700];
    const char* names[] = {"Indirizzo IP", "Segnale Wi-Fi"};
    const char* keys[] = {"ip", "rssi"};
    const char* units[] = {"", ",\"device_class\":\"signal_strength\",\"unit_of_measurement\":\"dBm\",\"state_class\":\"measurement\""};
    for (size_t i = 0; i < 2; ++i) {
        snprintf(topic, sizeof(topic), "homeassistant/sensor/%s/%s/config", deviceId, keys[i]);
        int length = snprintf(payload, sizeof(payload),
            "{\"name\":\"%s\",\"unique_id\":\"%s_%s\","
            "\"state_topic\":\"%s\",\"value_template\":\"{{ value_json.%s }}\","
            "\"availability_topic\":\"%s\",\"entity_category\":\"diagnostic\","
            "\"device\":{\"identifiers\":[\"%s\"],\"name\":\"termoFan %06x\","
            "\"manufacturer\":\"termoFan\",\"model\":\"NodeMCU 1.0\"}%s}",
            names[i], deviceId, keys[i], stateTopic, keys[i], availabilityTopic,
            deviceId, ESP.getChipId(), units[i]);
        if (length < 0 || static_cast<size_t>(length) >= sizeof(payload) ||
            !client.publish(topic, payload, true)) return false;
    }
    return true;
}

bool publishState() {
    char payload[100];
    snprintf(payload, sizeof(payload), "{\"ip\":\"%s\",\"rssi\":%ld}",
             WiFi.localIP().toString().c_str(), static_cast<long>(WiFi.RSSI()));
    return client.publish(stateTopic, payload, true);
}

void disconnect() {
    if (client.connected()) {
        client.publish(availabilityTopic, "offline", true);
        client.disconnect();
    } else {
        transport.stop();
    }
}

void receive(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, "homeassistant/status") == 0 && length == 6 &&
        memcmp(payload, "online", 6) == 0) {
        discoveryPending = true;
    }
}
}

const MqttConfig& Mqtt::config() { return broker; }

void Mqtt::configure(const MqttConfig& next) {
    disconnect();
    broker = next;
    snprintf(deviceId, sizeof(deviceId), "termofan_%06x", ESP.getChipId());
    snprintf(availabilityTopic, sizeof(availabilityTopic), "termofan/%s/availability", deviceId);
    snprintf(stateTopic, sizeof(stateTopic), "termofan/%s/state", deviceId);
    client.setServer(broker.host, broker.port);
    client.setSocketTimeout(2);
    client.setBufferSize(1024);
    client.setCallback(receive);
    attempted = false;
}

void Mqtt::update() {
    if (!WiFi.isConnected() || !broker.host[0]) {
        if (attempted) disconnect();
        attempted = false;
        return;
    }
    if (!client.connected()) {
        if (attempted && millis() - lastAttempt < 15000) return;
        attempted = true;
        char id[24];
        snprintf(id, sizeof(id), "termoFan-%06x", ESP.getChipId());
        if (client.connect(id, broker.username[0] ? broker.username : nullptr,
                           broker.password[0] ? broker.password : nullptr,
                           availabilityTopic, 1, true, "offline")) {
            if (!client.subscribe("homeassistant/status")) {
                disconnect();
                lastAttempt = millis();
                return;
            }
            discoveryPending = true;
            lastState = 0;
        }
        lastAttempt = millis();
        return;
    }
    client.loop();
    if (discoveryPending) {
        if (publishDiscovery() && publishState() &&
            client.publish(availabilityTopic, "online", true)) {
            discoveryPending = false;
            lastState = millis();
        } else {
            disconnect();
            lastAttempt = millis();
        }
    } else if (millis() - lastState >= 60000) {
        if (publishState()) lastState = millis();
    }
}

const char* Mqtt::status() {
    if (!broker.host[0]) return "Da configurare";
    if (!WiFi.isConnected()) return "In attesa del Wi-Fi";
    if (client.connected()) return "Connesso";
    if (!attempted) return "Connessione in corso";
    switch (client.state()) {
        case MQTT_CONNECT_BAD_CREDENTIALS:
        case MQTT_CONNECT_UNAUTHORIZED: return "Accesso rifiutato";
        case MQTT_CONNECT_BAD_PROTOCOL:
        case MQTT_CONNECT_BAD_CLIENT_ID: return "Connessione rifiutata";
        default: return "Disconnesso";
    }
}
