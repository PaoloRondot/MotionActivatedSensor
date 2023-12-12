#pragma once

#include <Arduino.h>

#include "capteur.hpp"

enum ULTRASON_SCENARIO: uint8_t {
    PLAY_ONCE_WHEN_WITHIN = 1,
    PLAY_ONCE_WHEN_WITHIN_MORE_THAN_X_SEC = 2,
    PLAY_WHILE_WITHIN = 3,
    PLAY_ONCE_WHEN_OUTSIDE = 4,
    PLAY_WHILE_OUTSIDE = 5,
    PLAY_ONCE_WHEN_WITHIN_MORE_THAN_X_SEC_AND_AGAIN_WHEN_STILL_WITHIN_MORE_THAN_Y_SEC = 6,
};

class Ultrason : public Capteur
{
public:
    enum ULTRASON_STATE: uint8_t {
        OUTSIDE = 0,
        INSIDE_TO_OUTSIDE = 1,
        INSIDE_FOR_X_SEC = 2,
        INSIDE_FOR_X_SEC_AND_AGAIN_FOR_Y_SEC = 3,
    };

    Ultrason(const int& delayMin, const int& delaySec, const uint8_t& echo_pin, const int& scenario, const uint8_t& trig_pin, const uint16_t& min_distance, const uint16_t& time_within_minimum_sec, const uint16_t& time_within_minimum_sec2);
    ~Ultrason();

    bool isTriggered(uint32_t &minutes_since_act, uint8_t &seconds_since_act, uint32_t seconds_since_boot_act_timestamp, const uint32_t &seconds_since_boot, PLAYER_STATE &player_state) override;
    void pickMusic() override;
private:
    uint8_t trig_pin_;
    uint16_t min_distance_;
    uint16_t time_within_minimum_sec_;
    uint16_t time_within_minimum_sec_2_;
    ULTRASON_STATE state_ {ULTRASON_STATE::OUTSIDE};

    uint16_t measureDistance_();
    bool stayedInside_(uint32_t& last_sucessful_try_timestamp_ms);
    bool logicTriggerTimeThresholdInside_(PLAYER_STATE& player_state, uint32_t& last_try_timestamp_ms, uint32_t& last_sucessful_try_timestamp_ms);
    void stateMachine_();
    void pickMusicSpecial_();
};