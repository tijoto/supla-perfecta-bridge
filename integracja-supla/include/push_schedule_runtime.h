#pragma once
#include <Arduino.h>
#include "push_schedule.h"
bool setupPushSchedule(uint16_t legacyFlags);
void servicePushClock();
bool scheduledPushAllowed(uint8_t rule);
uint16_t scheduledPushFlags();
bool saveLegacyPushFlags(uint16_t flags);
bool savePushSchedule(const String& dayStart,const String& dayEnd,const String& rules,String& error);
String pushScheduleJson();
