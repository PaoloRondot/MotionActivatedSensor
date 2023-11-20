#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

#include "capteur.hpp"

enum PIR_SCENARIO: uint8_t {
    PLAY_ONCE_WHEN_MOVE = 1,
    PLAY_WHILE_MOVE = 2,
    PLAY_ONCE_WHEN_NO_MOVE = 3,
    PLAY_WHILE_NO_MOVE = 4
};

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
