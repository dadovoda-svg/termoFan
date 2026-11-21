#pragma once
#include "WifiConfig.h"

namespace Mqtt {
const MqttConfig& config();
void configure(const MqttConfig& next);
void update();
const char* status();
}
