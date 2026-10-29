#pragma once
#include <Arduino.h>
void setupSupla();
void serviceSupla();
String suplaJson();
bool saveSuplaPushFlags(uint16_t flags);
bool queueSuplaTestPush();
