#pragma once

#include <Arduino.h>

#include "capteur.hpp"

class Infrarouge : public Capteur
{
private:
    int delayMin_;
    int delaySec_;
    uint8_t pin_;
    int scenario_;
    bool running_;
    int loops_since_act_;
public:
    Infrarouge(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario);
    ~Infrarouge();

    virtual bool isTriggered(unsigned long& delaySinceActMin, unsigned long& delaySinceActSec, unsigned long& timeLast2, const unsigned long& timeNow,  bool& letsgo);
};