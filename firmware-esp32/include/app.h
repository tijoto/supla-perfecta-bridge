#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_timer.h>
#include <driver/gpio.h>
#include <time.h>
#include "protocol.h"
#include "private_config.h"
#include "z7_config.h"
constexpr uint8_t ARM_PIN=25,DISARM_PIN=26,RX_PIN=16,TX_PIN=17;
constexpr uint32_t PULSE_MS=300,PULSE_US=PULSE_MS*1000;
constexpr uint8_t OUTPUT_ACTIVE_LEVEL=0,OUTPUT_IDLE_LEVEL=1;
constexpr uint16_t BRIDGE_PORT=2217;
constexpr const char* VERSION="Perfecta-ESP Community 1.0.0";
struct History{uint32_t magic;uint32_t violation[16];uint32_t battery[16];};
struct AppState{
 bool ready=false,hasStatus=false,hasDiag=false,hasRf=false,namesReady=false,selfTest=false;
 uint8_t status[61]={},diag[137]={},rf[44]={},config[6177]={};
 uint32_t statusAt=0,diagAt=0,rfAt=0,sessionCount=0,crcErrors=0,timeouts=0,samples=0,txBytes=0,rxBytes=0;
 uint32_t bridgeTx=0,bridgeRx=0,bridgeAt=0,rejectedClients=0;
 uint32_t lastHistorySave=0;bool historyDirty=false;
 bool bridge=false,monitorPaused=false;String error,mode="reader";
 History history={};
};
extern AppState app;
extern HardwareSerial panel;
extern WiFiServer bridgeServer;
extern WiFiClient bridgeClient;
extern WebServer web;
extern Preferences prefs;
extern String csrfToken;
extern volatile uint8_t activeOutput;
extern uint32_t lastPulseAt,pulseCount;
extern volatile uint32_t pulseEndedAt;
void setupPanel();
void servicePanel();
void setupWeb();
void serviceWeb();
void setupOutputs();
bool startPulse(uint8_t pin);
String stateJson();
String jquote(const String& value);
String panelName(size_t offset);
uint32_t panelEpoch();
void processPayload(const uint8_t* p,size_t n);
void historyTick();
