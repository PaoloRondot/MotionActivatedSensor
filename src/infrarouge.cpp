#include "infrarouge.hpp"

#define MIN_LOOP 1000
#define MIN_LOOP_2 100

Infrarouge::Infrarouge(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario)
:Capteur(delayMin, delaySec, pin, scenario)
{
}

Infrarouge::~Infrarouge()
{
}

bool Infrarouge::isTriggered(uint32_t &minutes_since_act, uint8_t &seconds_since_act, uint32_t seconds_since_boot_act_timestamp, const uint32_t& seconds_since_boot, PLAYER_STATE &player_state) {
    switch (scenario_)
    {
    case 1:
        if (((minutes_since_act >= delayMin_ && seconds_since_act >= delaySec_) && !digitalRead (pin_)) || player_state == PLAYER_STATE::PLAYING)
            return true;
        else return false;
        break;

    case 2:
        if (((minutes_since_act >= delayMin_ && seconds_since_act >= delaySec_)  && !digitalRead(pin_)) || (running_ && loops_since_act_ < MIN_LOOP)) {
            running_ = true;
            if (!digitalRead (pin_)) loops_since_act_ = 0;
            loops_since_act_ ++;
            return true;
        }
        else {
            running_ = false;
            return false;
        }
        break;

    case 3:
        if (((minutes_since_act >= delayMin_ && seconds_since_act >= delaySec_) && digitalRead (pin_)) || player_state == PLAYER_STATE::PLAYING)
            return true;
        else return false;
        break;

    case 4:
        if (((minutes_since_act >= delayMin_ && seconds_since_act >= delaySec_)  && digitalRead (pin_)) || (running_ && loops_since_act_ < MIN_LOOP_2)) {
            running_ = true;
            if (digitalRead(pin_)) loops_since_act_ = 0;
            loops_since_act_ ++;
            return true;
        }
        else {
            running_ = false;
            return false;
        }
        break;

    default:
        return false;
        break;
    }
}