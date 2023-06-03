#include "bouton.hpp"

Bouton::Bouton(const uint8_t& pin, const int& scenario):pin_(pin), scenario_(scenario), running_(false), loops_since_act_(0)
{
}

Bouton::~Bouton()
{
}

bool Bouton::isTriggered(const int& delaySinceActMin, const int& delaySinceActSec, bool& letsgo) {
    switch (scenario_)
    {
    case 1:
        if (letsgo) {
            if (digitalRead(pin_)) {
                while (digitalRead(pin_));
                letsgo = false;
            }
            return true;
        }
        else if (digitalRead(pin_)){
            while (digitalRead(pin_));
            return true;
        }
        else return false;
        break;

    case 2:
        if (digitalRead(pin_)) {
            return true;
        }
        else {
            return false;
        }
        break;

    default:
        return false;
        break;
    }
}