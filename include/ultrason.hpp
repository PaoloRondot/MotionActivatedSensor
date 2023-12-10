#pragma once

#include <Arduino.h>

#include "capteur.hpp"

enum ULTRASON_SCENARIO: uint8_t {
    PLAY_ONCE_WHEN_WITHIN = 1,
    PLAY_WHILE_WITHIN = 2,
    PLAY_ONCE_WHEN_OUTSIDE = 3,
    PLAY_WHILE_OUTSIDE = 4,
};

class Ultrason : public Capteur
{
public:
    Ultrason(const int& delayMin, const int& delaySec, const uint8_t& echo_pin, const int& scenario, const uint8_t& trig_pin, const uint16_t& min_distance);
    ~Ultrason();

    virtual bool isTriggered(uint32_t &minutes_since_act, uint8_t &seconds_since_act, uint32_t seconds_since_boot_act_timestamp, const uint32_t &seconds_since_boot, PLAYER_STATE &player_state) override;
private:
    uint8_t trig_pin_;
    uint16_t min_distance_;
};