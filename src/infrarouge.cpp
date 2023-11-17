#include "infrarouge.hpp"

#define MIN_LOOP 1000
#define MIN_LOOP_2 100

Infrarouge::Infrarouge(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario)
:delayMin_(delayMin), delaySec_(delaySec), pin_(pin), scenario_(scenario), running_(false), loops_since_act_(0)
{
}

Infrarouge::~Infrarouge()
{
}

bool Infrarouge::isTriggered(unsigned long& delaySinceActMin, unsigned long& delaySinceActSec, unsigned long& timeLast2, const unsigned long& timeNow,  bool& letsgo){
    switch (scenario_)
    {
    case 1:
        if (((delaySinceActMin >= delayMin_ && delaySinceActSec >= delaySec_) && !digitalRead (pin_)) || letsgo)
            return true;
        else return false;
        break;

    case 2:
        if (((delaySinceActMin >= delayMin_ && delaySinceActSec >= delaySec_)  && !digitalRead(pin_)) || (running_ && loops_since_act_ < MIN_LOOP)) {
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
        if (((delaySinceActMin >= delayMin_ && delaySinceActSec >= delaySec_) && digitalRead (pin_)) || letsgo)
            return true;
        else return false;
        break;

    case 4:
        if (((delaySinceActMin >= delayMin_ && delaySinceActSec >= delaySec_)  && digitalRead (pin_)) || (running_ && loops_since_act_ < MIN_LOOP_2)) {
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