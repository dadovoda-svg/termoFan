#include "Fans.h"
#include <Arduino.h>

namespace {
constexpr uint8_t pwmPins[] = {D1, D2, D0};
constexpr uint8_t tachPins[] = {D5, D6, D7};
volatile uint32_t pulses[3] = {};
unsigned long lastSample;

void IRAM_ATTR tach1() { ++pulses[0]; }
void IRAM_ATTR tach2() { ++pulses[1]; }
void IRAM_ATTR tach3() { ++pulses[2]; }
}

void Fans::begin() {
    for (uint8_t pin : pwmPins) {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
    }
    for (uint8_t pin : tachPins) pinMode(pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(tachPins[0]), tach1, FALLING);
    attachInterrupt(digitalPinToInterrupt(tachPins[1]), tach2, FALLING);
    attachInterrupt(digitalPinToInterrupt(tachPins[2]), tach3, FALLING);
    lastSample = millis();
}

void Fans::update() {
    const unsigned long now = millis();
    const unsigned long elapsed = now - lastSample;
    if (elapsed < 5000) return;
    uint32_t counts[3];
    noInterrupts();
    for (unsigned i = 0; i < 3; ++i) {
        counts[i] = pulses[i];
        pulses[i] = 0;
    }
    interrupts();
    lastSample = now;
    Serial.printf("Ventole RPM (2 impulsi/giro ipotizzati): %lu, %lu, %lu\r\n",
                  (unsigned long)(counts[0] * 60000ULL / elapsed / 2),
                  (unsigned long)(counts[1] * 60000ULL / elapsed / 2),
                  (unsigned long)(counts[2] * 60000ULL / elapsed / 2));
}
