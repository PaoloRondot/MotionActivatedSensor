#pragma once

#include <Arduino.h>

#include "capteur.hpp"

class Infrarouge : public Capteur
{
public:
    Infrarouge(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario);
    ~Infrarouge();

    bool isTriggered(uint32_t &minutes_since_act, uint8_t &seconds_since_act, uint32_t seconds_since_boot_act_timestamp, const uint32_t &seconds_since_boot, PLAYER_STATE &player_state) override;
};