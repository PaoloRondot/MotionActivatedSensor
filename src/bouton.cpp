#include "bouton.hpp"

Bouton::Bouton(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario)
:Capteur(delayMin, delaySec, pin, scenario)
{
}

Bouton::~Bouton()
{
}

bool Bouton::isTriggered(uint32_t &minutes_since_act, uint8_t &seconds_since_act, uint32_t seconds_since_boot_act_timestamp, const uint32_t &seconds_since_boot, PLAYER_STATE &player_state) {
    switch (scenario_)
    {
    case BOUTON_SCENARIO::PLAY_AND_RESTART:
        if (player_state == PLAYER_STATE::PLAYING) {
            if (digitalRead(pin_)) {
                while (digitalRead(pin_)) {
                    delay(10);
                }
                player_state = PLAYER_STATE::STOPPED;
            }
            return true;
        }
        else if (digitalRead(pin_)){
            while (digitalRead(pin_)) {
                delay(10);
            }
            return true;
        }
        else {
            return false;
        }
        break;

    case BOUTON_SCENARIO::PLAY_WHEN_PRESSED_AND_RESUME:
        if (digitalRead(pin_)) {
            return true;
        }
        else {
            if (player_state == PLAYER_STATE::PLAYING) {
                seconds_since_act = 0;
                minutes_since_act = 0;
                player_state = PLAYER_STATE::PAUSED;
            }
            return false;
        }
        break;

    case BOUTON_SCENARIO::PLAY_WHEN_PRESSED_AND_RESTART:
        if (digitalRead(pin_)) {
            return true;
        }
        else {
            if (player_state == PLAYER_STATE::PLAYING) {
                seconds_since_act = 0;
                minutes_since_act = 0;
                player_state = PLAYER_STATE::STOPPED;
            }
            return false;
        }
        break;

    case BOUTON_SCENARIO::PLAY_WHEN_PRESSED_AND_RESTART_AFTER_DELAY:
        if ((minutes_since_act * 60 + seconds_since_act >= delayMin_ * 60 + delaySec_) && digitalRead(pin_)) {
            return true;
        }
        else {
            if (player_state == PLAYER_STATE::PLAYING) {
                seconds_since_act = 0;
                minutes_since_act = 0;
                player_state = PLAYER_STATE::STOPPED;
            }
            return false;
        }
        break;

    default:
        return false;
        break;
    }
}
