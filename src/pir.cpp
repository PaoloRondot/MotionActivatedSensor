#include "pir.hpp"

#define MIN_LOOP 10000
#define MIN_LOOP_2 100

Pir::Pir(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario)
:Capteur(delayMin, delaySec, pin, scenario)
{
}

Pir::~Pir()
{
}

bool Pir::isTriggered(uint32_t &minutes_since_act, uint8_t &seconds_since_act, uint32_t seconds_since_boot_act_timestamp, const uint32_t &seconds_since_boot, PLAYER_STATE &player_state) {
    switch (scenario_)
    {
    case PIR_SCENARIO::PLAY_ONCE_WHEN_MOVE:
        if (((minutes_since_act * 60 + seconds_since_act >= delayMin_ * 60 + delaySec_) && digitalRead (pin_)) || player_state == PLAYER_STATE::PLAYING)
            return true;
        else return false;
        break;

    case PIR_SCENARIO::PLAY_WHILE_MOVE:
        if (((minutes_since_act >= delayMin_ && seconds_since_act >= delaySec_)  && digitalRead (pin_)) || (running_ && loops_since_act_ < MIN_LOOP)) {
            running_ = true;
            if (digitalRead (pin_)) loops_since_act_ = 0;
            loops_since_act_ ++;
            return true;
        }
        else {
            running_ = false;
            return false;
        }
        break;

    case PIR_SCENARIO::PLAY_ONCE_WHEN_NO_MOVE:
        if (((minutes_since_act >= delayMin_ && seconds_since_act >= delaySec_) && !digitalRead (pin_)) || player_state == PLAYER_STATE::PLAYING)
            return true;
        else return false;
        break;

    case PIR_SCENARIO::PLAY_WHILE_NO_MOVE:
        if (((minutes_since_act >= delayMin_ && seconds_since_act >= delaySec_)  && !digitalRead (pin_)) || (running_ && loops_since_act_ < MIN_LOOP_2)) {
            running_ = true;
            if (!digitalRead(pin_)) loops_since_act_ = 0;
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