#pragma once
#include <Arduino.h>
bool z7Busy();
String z7Json();
bool requestZ7Partition1(bool enabled,uint32_t revision,String& error,bool prepareOnly=false,bool quickPrepare=false);
bool z7Backup(uint8_t* output,size_t length);
bool requestInputPartitions(uint8_t number,bool partition1,bool partition2,uint32_t revision,String& error);
String inputPartitionJson(uint8_t number);
