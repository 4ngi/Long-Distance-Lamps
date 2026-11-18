#pragma once
#include <stdint.h>
void Mqtt_Begin(void (*messageHandler)(const char*));
void Mqtt_Update();
bool Mqtt_SendMessage(const char* text);
bool Mqtt_isConnected();
bool Mqtt_HasBeen(const uint32_t time);