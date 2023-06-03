#pragma once

#include <Arduino.h>

#include "capteur.hpp"

class Bouton : public Capteur
{
private:
    uint8_t pin_;
    int scenario_;
    bool running_;
    int loops_since_act_;
public:
    Bouton(const uint8_t& pin, const int& scenario);
    ~Bouton();

    virtual bool isTriggered(const int& delaySinceActMin, const int& delaySinceActSec, bool& letsgo);
};