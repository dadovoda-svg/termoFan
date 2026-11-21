#include "WifiPortal.h"
#include "WifiConfig.h"
#include "Mqtt.h"
#include "PortalStyle.h"
#include "Logger.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <cctype>

namespace {
constexpr unsigned long connectedLedPeriodMs = 20000;
constexpr unsigned long connectedLedPulseMs = 50;
constexpr uint8_t flashButton = 0;
constexpr unsigned long trialTimeout = 30000;
ESP8266WebServer web(80);
DNSServer dns;
Logger* logOutput;
WifiConfig saved;
WifiConfig candidate;
enum class Trial { idle, queued, connecting };
Trial trial = Trial::idle;
unsigned long trialStart;
bool captive = false;
bool storageReady;
bool previouslyConnected = false;
String formToken;
String notice;

String randomToken() {
    char token[25];
    snprintf(token, sizeof(token), "%08x%08x%08x", ESP.random(), ESP.random(), ESP.random());
    return String(token);
}

String escaped(String value) {
    value.replace("&", "&amp;");
    value.replace("<", "&lt;");
    value.replace(">", "&gt;");
    value.replace("\"", "&quot;");
    return value;
}

bool authorized() {
    web.sendHeader("Cache-Control", "no-store");
    if (web.authenticate("admin", saved.adminPassword)) return true;
    web.requestAuthentication(DIGEST_AUTH, "termoFan");
    return false;
}

void startPortal() {
    if (captive) return;
    String name = "termoFan-" + String(ESP.getChipId(), HEX);
    String password = randomToken();
    const IPAddress portalIP(10, 77, 0, 1);
    const IPAddress subnetMask(255, 255, 255, 0);
    if (!WiFi.softAPConfig(portalIP, portalIP, subnetMask) ||
        !WiFi.softAP(name.c_str(), password.c_str())) {
        WiFi.enableAP(false);
        logOutput->printf(Logger::Level::ERROR, "Avvio access point fallito\r\n");
        return;
    }
    captive = true;
    dns.start(53, "*", WiFi.softAPIP());
    // The setup password is issued only on the physical serial console.
    logOutput->printf(Logger::Level::INFO, "AP %s | password temporanea %s | IP %s\r\n",
                      name.c_str(), password.c_str(), WiFi.softAPIP().toString().c_str());
}

void showPage() {
    if (!authorized()) return;
    String html = F("<!doctype html><html lang='it'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>termoFan · Configurazione</title><link rel='stylesheet' href='/portal.css'></head><body><header><div class='wrap'><strong>termoFan</strong><span>Portale locale</span></div></header><main><h1>Configurazione</h1><div class='status'><div><small>Rete Wi-Fi</small><b>");
    html += WiFi.isConnected() ? escaped(WiFi.SSID()) + " · " + WiFi.localIP().toString() : String("Scollegata");
    html += F("</b></div><div><small>Broker MQTT</small><b id='mqtt' aria-live='polite'>");
    html += Mqtt::status();
    html += F("</b></div></div>");
    if (notice.length()) html += "<p class='notice'>" + escaped(notice) + "</p>";
    if (trial != Trial::idle) html += F("<p>Verifica della nuova rete in corso.</p>");
    String hidden = "<input type='hidden' name='token' value='" + formToken + "'>";
    const auto& broker = Mqtt::config();
    html += F("<div class='grid'><section><h2>Broker MQTT</h2><form method='post' action='/mqtt'><label>Indirizzo del broker<input name='host' required maxlength='253' placeholder='Nome host o IP' value=\"");
    html += escaped(broker.host);
    html += F("\"></label><label>Porta<input type='number' name='port' min='1' max='65535' required value='");
    html += String(broker.port);
    html += F("'></label><label>Username<input name='user' maxlength='64' value=\"");
    html += escaped(broker.username);
    html += F("\"></label><label>Password<input type='password' name='pass' maxlength='128' autocomplete='new-password'></label>");
    if (broker.password[0]) html += F("<p class='hint'>Lascia vuota la password per conservare quella salvata sullo stesso broker. Svuota anche lo username per usare l’accesso anonimo.</p>");
    html += hidden + "<button>Salva broker</button></form></section>";
    html += F("<section><h2>Rete Wi-Fi</h2><form method='post' action='/wifi'><label>Nome rete (SSID)<input name='ssid' list='reti' maxlength='32' required></label><datalist id='reti'>");
    if (trial == Trial::idle) {
        int count = WiFi.scanNetworks();
        for (int i = 0; i < count; ++i) {
            html += "<option value=\"" + escaped(WiFi.SSID(i)) + "\"></option>";
        }
        WiFi.scanDelete();
    }
    html += F("</datalist><label>Password Wi-Fi<input type='password' name='pass' maxlength='64'></label><p class='hint'>Lascia vuoto per una rete aperta.</p>");
    html += hidden + "<button>Verifica e salva rete</button></form></section></div>";
    html += F("<section><h2>Accesso amministratore</h2><form method='post' action='/admin'><label>Nuova password per admin<input type='password' name='pass' autocomplete='new-password' minlength='8' maxlength='64' required></label>");
    html += hidden + "<button>Cambia password</button></form></section></main>";
    html += F("<script>async function refresh(){const el=document.getElementById('mqtt');try{const r=await fetch('/mqtt/status');if(!r.ok)throw Error();el.textContent=await r.text()}catch{el.textContent='Stato non disponibile'}setTimeout(refresh,5000)}setTimeout(refresh,5000)</script></body></html>");
    web.send(200, "text/html; charset=utf-8", html);
}

bool allowChange() {
    if (!authorized()) return false;
    if (web.arg("token") != formToken) {
        web.send(403, "text/plain", "Ricaricare la pagina.");
        return false;
    }
    if (trial != Trial::idle || !storageReady) {
        web.send(503, "text/plain", "Configurazione momentaneamente indisponibile.");
        return false;
    }
    return true;
}

void submitWifi() {
    if (!allowChange()) return;
    String ssid = web.arg("ssid");
    String pass = web.arg("pass");
    bool validKey = pass.isEmpty() || (pass.length() >= 8 && pass.length() <= 63);
    if (pass.length() == 64) {
        validKey = true;
        for (char c : pass) validKey &= isxdigit(static_cast<unsigned char>(c)) != 0;
    }
    if (ssid.isEmpty() || ssid.length() > 32 || strlen(ssid.c_str()) != ssid.length()
        || strlen(pass.c_str()) != pass.length() || !validKey) {
        web.send(400, "text/plain", "SSID o password Wi-Fi non validi.");
        return;
    }
    candidate = saved;
    ssid.toCharArray(candidate.ssid, sizeof(candidate.ssid));
    pass.toCharArray(candidate.password, sizeof(candidate.password));
    trial = Trial::queued;
    trialStart = millis();
    web.send(202, "text/plain; charset=utf-8", "Verifica avviata. Se riesce, aprire il nuovo IP indicato sul monitor seriale. In caso di errore riaprire il portale precedente dopo 30 secondi.");
}

void submitMqtt() {
    if (!allowChange()) return;
    String host = web.arg("host");
    host.trim();
    const String port = web.arg("port");
    const String user = web.arg("user");
    const String pass = web.arg("pass");
    MqttConfig next;
    bool validHost = host.length() && host.length() < sizeof(next.host);
    for (unsigned char c : host) {
        if (!isalnum(c) && c != '-' && c != '.') validHost = false;
    }
    bool validPort = port.length() && port.length() <= 5;
    for (unsigned char c : port) {
        if (!isdigit(c)) validPort = false;
    }
    long number = port.toInt();
    if (!validHost || !validPort || number < 1 || number > 65535 ||
        user.length() >= sizeof(next.username) || pass.length() >= sizeof(next.password) ||
        user.length() != strlen(user.c_str()) || pass.length() != strlen(pass.c_str()) ||
        (user.isEmpty() && pass.length())) {
        web.send(400, "text/plain", "Verificare indirizzo, porta e credenziali MQTT.");
        return;
    }
    next.port = number;
    host.toCharArray(next.host, sizeof(next.host));
    user.toCharArray(next.username, sizeof(next.username));
    const auto& old = Mqtt::config();
    if (user.length() && pass.isEmpty() && old.password[0]) {
        if (!host.equalsIgnoreCase(old.host) || number != old.port || user != old.username) {
            web.send(400, "text/plain", "Reinserire la password per il nuovo broker o utente.");
            return;
        }
        memcpy(next.password, old.password, sizeof(next.password));
    } else {
        pass.toCharArray(next.password, sizeof(next.password));
    }
    if (!saveMqttConfig(next)) {
        web.send(500, "text/plain", "Salvataggio del broker fallito.");
        return;
    }
    Mqtt::configure(next);
    notice = F("Configurazione MQTT salvata.");
    web.sendHeader("Location", "/");
    web.send(303);
}

void submitAdmin() {
    if (!allowChange()) return;
    String pass = web.arg("pass");
    if (pass.length() < 8 || pass.length() > 64 || pass.length() != strlen(pass.c_str())) {
        web.send(400, "text/plain", "Usare da 8 a 64 byte.");
        return;
    }
    WifiConfig next = saved;
    memset(next.adminPassword, 0, sizeof(next.adminPassword));
    pass.toCharArray(next.adminPassword, sizeof(next.adminPassword));
    if (!saveWifiConfig(next)) {
        web.send(500, "text/plain", "Salvataggio fallito.");
        return;
    }
    saved = next;
    formToken = randomToken();
    web.send(200, "text/plain", "Password aggiornata. Riaprire il portale.");
}

void updateTrial() {
    if (trial == Trial::idle) return;
    if (trial == Trial::queued) {
        if (millis() - trialStart < 500) return;
        WiFi.disconnect();
        WiFi.begin(candidate.ssid, candidate.password);
        trial = Trial::connecting;
        trialStart = millis();
        return;
    }
    bool connected = WiFi.isConnected() && WiFi.SSID() == candidate.ssid
        && WiFi.localIP() != IPAddress(0, 0, 0, 0);
    if (!connected && millis() - trialStart < trialTimeout) return;
    if (connected && saveWifiConfig(candidate)) {
        saved = candidate;
        notice = F("Rete verificata e salvata.");
        if (captive) {
            if (WiFi.enableAP(false)) {
                dns.stop();
                captive = false;
            } else {
                // The verified configuration is already durable. Boot into STA mode.
                ESP.restart();
            }
        }
        previouslyConnected = false;
    } else {
        notice = connected ? F("Salvataggio fallito.") : F("Connessione fallita. Controllare rete e password.");
        logOutput->printf(Logger::Level::ERROR, "%s\r\n", notice.c_str());
        WiFi.disconnect();
        if (saved.ssid[0]) WiFi.begin(saved.ssid, saved.password);
    }
    candidate = WifiConfig{};
    trial = Trial::idle;
}

void updateButton() {
    static unsigned long pressedAt;
    static bool pressed;
    static bool handled;
    if (digitalRead(flashButton) == HIGH) {
        pressed = handled = false;
        return;
    }
    if (!pressed) {
        pressedAt = millis();
        pressed = true;
    }
    if (!handled && millis() - pressedAt >= 5000) {
        startPortal();
        handled = true;
    }
}
}

void WifiPortal::begin(Logger& logger) {
    logOutput = &logger;
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    pinMode(flashButton, INPUT_PULLUP);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    formToken = randomToken();
    storageReady = LittleFS.begin();
    if (storageReady && loadWifiConfig(saved) && saved.ssid[0]) {
        WiFi.begin(saved.ssid, saved.password);
    } else {
        if (!storageReady) notice = F("Memoria configurazione indisponibile.");
        startPortal();
    }
    MqttConfig broker;
    if (storageReady && loadMqttConfig(broker)) Mqtt::configure(broker);
    web.on("/portal.css", HTTP_GET, [] {
        if (authorized()) web.send_P(200, "text/css", portalStyle);
    });
    web.on("/mqtt/status", HTTP_GET, [] {
        if (authorized()) web.send(200, "text/plain; charset=utf-8", Mqtt::status());
    });
    web.on("/mqtt", HTTP_POST, submitMqtt);
    web.on("/", HTTP_GET, showPage);
    web.on("/wifi", HTTP_POST, submitWifi);
    web.on("/admin", HTTP_POST, submitAdmin);
    web.onNotFound([] {
        if (captive && web.client().localIP() == WiFi.softAPIP() && web.method() == HTTP_GET) {
            web.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
            web.send(302, "text/plain", "");
        } else if (authorized()) {
            web.send(404, "text/plain", "Pagina sconosciuta.");
        }
    });
    web.begin();
}

void WifiPortal::update() {
    updateButton();
    if (captive) dns.processNextRequest();
    web.handleClient();
    updateTrial();
    if (trial == Trial::idle) Mqtt::update();
    bool connected = WiFi.isConnected();
    if (connected && !previouslyConnected) {
        logOutput->printf(Logger::Level::INFO, "Wi-Fi connesso | IP %s\r\n", WiFi.localIP().toString().c_str());
    }
    previouslyConnected = connected;
    const unsigned long now = millis();
    bool ledOn;
    if (captive) {
        ledOn = now % 500 < 250;
    } else if (connected) {
        ledOn = now % connectedLedPeriodMs < connectedLedPulseMs;
    } else {
        ledOn = now % 1500 < 150;
    }
    digitalWrite(LED_BUILTIN, ledOn ? LOW : HIGH);
}
