#include <Arduino.h>
#include "Logger.h"
#include "WifiPortal.h"
#include "Fans.h"

namespace {
Logger logger(Serial);
}

void setup() {
    Serial.begin(115200);
    Fans::begin();
    logger.printf(Logger::Level::INFO, "\r\ntermoFan - avvio completato\r\n");
    WifiPortal::begin(logger);
}

void loop() {
    WifiPortal::update();
    Fans::update();
    delay(10);
}
