#include "bouton.hpp"

Bouton::Bouton(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario)
:delayMin_(delayMin), delaySec_(delaySec), pin_(pin), scenario_(scenario), running_(false), loops_since_act_(0)
{
}

Bouton::~Bouton()
{
}

bool Bouton::isTriggered(unsigned long& delaySinceActMin, unsigned long& delaySinceActSec, unsigned long& timeLast2, const unsigned long& timeNow,  bool& letsgo) {
    switch (scenario_)
    {
    case BOUTON_SCENARIO::PLAY_WHEN_PRESSED_AND_RESUME:
        if (letsgo) {
            if (digitalRead(pin_)) {
                while (digitalRead(pin_)) {
                    delay(10);
                }
                letsgo = false;
            }
            return true;
        }
        else if (digitalRead(pin_)){
            Serial.println("I'm here three");
            while (digitalRead(pin_)) {
                delay(10);
            }
            return true;
        }
        else {
            return false;
        }
        break;

    case BOUTON_SCENARIO::PLAY_WHEN_PRESSED_AND_RESTART:
        if (digitalRead(pin_)) {
            return true;
        }
        else {
            return false;
        }
        break;

    case BOUTON_SCENARIO::PLAY_WHEN_PRESSED_AND_RESTART_AFTER_DELAY:
        // Serial.println("delaySinceActMin : " + String(delaySinceActMin) + " delaySinceActSec : " + String(delaySinceActSec) + " digitalRead(pin_) : " + String(digitalRead(pin_)) + " letsgo : " + String(letsgo));
        if ((delaySinceActMin >= delayMin_ && delaySinceActSec >= delaySec_) && digitalRead(pin_)) {
            return true;
        }
        else {
            if (letsgo) {
                timeLast2 = timeNow;
                delaySinceActSec = 0;
                delaySinceActMin = 0;
                letsgo = false;
            }
            return false;
        }
        break;

    default:
        return false;
        break;
    }
}