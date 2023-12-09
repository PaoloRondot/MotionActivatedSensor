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

class Pir : public Capteur
{
public:
    Pir(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario);
    ~Pir();

    virtual bool isTriggered(uint32_t &minutes_since_act, uint8_t &seconds_since_act, uint32_t seconds_since_boot_act_timestamp, const uint32_t &seconds_since_boot,  PLAYER_STATE &player_state) override;
};
