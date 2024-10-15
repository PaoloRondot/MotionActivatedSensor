#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include "logger.hpp"

#define NB_SON 25
enum PLAYER_STATE { PLAYING, PAUSED, STOPPED, WAITING };
enum CAPTEUR_TYPE: uint8_t {
    PIR = 1,
    BOUTON = 2,
    INFRAROUGE = 3,
    ULTRASON = 4
};

class Capteur
{
public:
    Capteur(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario);
    ~Capteur();

    virtual bool isTriggered(uint32_t &minutes_since_act, uint8_t &seconds_since_act, uint32_t seconds_since_boot_act_timestamp, const uint32_t &seconds_since_boot, PLAYER_STATE &player_state) = 0;
    virtual void pickMusic();

    void setMaxSound(const uint8_t& max_sound);
    uint8_t getCurrentIndex() const { return current_index_; }

    void updateDelay(const int& delayMin, const int& delaySec);

protected:
    void randomizeAll_();

    uint16_t delayMin_{0};
    uint16_t delaySec_{0};
    uint8_t pin_{0};
    uint8_t scenario_{0};
    uint8_t current_index_{0};
    bool running_;
    uint8_t loops_since_act_;

    uint8_t randomized_indexes_[NB_SON];

private:
    uint8_t max_sound_ = 0;
};