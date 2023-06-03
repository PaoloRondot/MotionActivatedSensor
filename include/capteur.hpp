#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

class Capteur
{
public:
    // Capteur();
    virtual ~Capteur() = default;
    virtual bool isTriggered(const int& delaySinceActMin, const int& delaySinceActSec, bool& letsgo) = 0;
};