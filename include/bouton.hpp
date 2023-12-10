#pragma once

#include <Arduino.h>

#include "capteur.hpp"

enum BOUTON_SCENARIO: uint8_t {
    PLAY_AND_RESTART = 1,
    PLAY_WHEN_PRESSED_AND_RESUME = 2,
    PLAY_WHEN_PRESSED_AND_RESTART = 3,
    PLAY_WHEN_PRESSED_AND_RESTART_AFTER_DELAY = 4
};

class Bouton : public Capteur
{
public:
    Bouton(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario);
    ~Bouton();

    bool isTriggered(uint32_t &minutes_since_act, uint8_t &seconds_since_act, uint32_t seconds_since_boot_act_timestamp, const uint32_t &seconds_since_boot, PLAYER_STATE &player_state) override;
};