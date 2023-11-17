#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#include "capteur.hpp"

class PIR : public Capteur
{
private:
    int delayMin_;
    int delaySec_;
    uint8_t pin_;
    int scenario_;
    bool running_;
    int loops_since_act_;
public:
    PIR(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario);
    ~PIR();

    virtual bool isTriggered(unsigned long& delaySinceActMin, unsigned long& delaySinceActSec, unsigned long& timeLast2, const unsigned long& timeNow,  bool& letsgo);
};
