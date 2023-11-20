#pragma once

#include <Arduino.h>

#include "capteur.hpp"

enum BOUTON_SCENARIO: uint8_t {
    PLAY_WHEN_PRESSED_AND_RESUME = 1,
    PLAY_WHEN_PRESSED_AND_RESTART = 2,
    PLAY_WHEN_PRESSED_AND_RESTART_AFTER_DELAY = 3
};

class Bouton : public Capteur
{
private:
    int delayMin_;
    int delaySec_;
    uint8_t pin_;
    int scenario_;
    bool running_;
    int loops_since_act_;
public:
    Bouton(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario);
    ~Bouton();

    virtual bool isTriggered(unsigned long& delaySinceActMin, unsigned long& delaySinceActSec, unsigned long& timeLast2, const unsigned long& timeNow,  bool& letsgo);
};