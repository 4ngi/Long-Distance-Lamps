#pragma once

#include <Arduino.h>

//setup
void Lamp_Begin();
//in loop: checks connections, ...
void Lamp_Update(bool canDiscover);

//getters
bool Lamp_isOn();
bool Lamp_IsDimToWarm();

//setters and actions
int Lamp_Toggle();
int Lamp_ToggleDimToWarm();
int Lamp_AdjustTemperature(int direction);
int Lamp_AdjustBrightness(int direction);

int Lamp_Pulse();
void Lamp_ShowChangeOfState();

//claims
void Lamp_AddClaim(const String& mac, uint32_t now);
String Lamp_GetAssignedMac();

