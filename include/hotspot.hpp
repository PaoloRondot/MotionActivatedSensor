#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

extern ESP8266WebServer server;
extern String st;
extern String content;
extern int statusCode;
extern const char* ssid;
extern const char* passphrase;

bool testWifi(void);
void launchWeb();
void setupAP(void);
void createWebServer();

