#include "ultrason.hpp"

constexpr float SOUND_SPEED = 0.034;
constexpr uint16_t CHECK_INTERVAL = 30;
extern Logger *logger;

Ultrason::Ultrason(const int& delayMin, const int& delaySec,
                   const uint8_t& echo_pin, const int& scenario,
                   const uint8_t& trig_pin, const uint16_t& min_distance,
                   const uint16_t& time_within_minimum_sec, 
                   const uint16_t& time_within_minimum_sec2)
    : Capteur(delayMin, delaySec, echo_pin, scenario),
      trig_pin_(trig_pin),
      min_distance_(min_distance),
      time_within_minimum_sec_(time_within_minimum_sec),
      time_within_minimum_sec_2_(time_within_minimum_sec2) {}

Ultrason::~Ultrason() {}

bool Ultrason::isTriggered(uint32_t& minutes_since_act,
                           uint8_t& seconds_since_act,
                           uint32_t seconds_since_boot_act_timestamp,
                           const uint32_t& seconds_since_boot,
                           PLAYER_STATE& player_state) {
    static PLAYER_STATE last_player_state = PLAYER_STATE::STOPPED;

    // When the player changes to the stopped state, and we are in PLAY_WHILE_WITHIN, we want to give a chance to keep playing
    // If the player is stopped and the delay before a new play is not reached, do not play
    if (
        ((player_state == PLAYER_STATE::STOPPED) && delayReached_(minutes_since_act, seconds_since_act)) 
        && !(playingToStopped_(player_state, last_player_state) && scenario_ == ULTRASON_SCENARIO::PLAY_WHILE_WITHIN)) {
        last_player_state = player_state;
        return false;
    }

    switch (scenario_) {
        case ULTRASON_SCENARIO::PLAY_WHILE_WITHIN: {
            // Keep playing if the player is playing and still within the distance
            if (player_state == PLAYER_STATE::PLAYING) {
                last_player_state = player_state;
                // If the player is playing, we do not want to check the distance too often
                if ((millis() - last_try_timestamp_ms_ < CHECK_INTERVAL)) {
                    return true;
                } 
                // If we're still within the distance, keep playing
                else if (measureDistance_() <= min_distance_) {
                    return true;
                } else {
                    player_state = PLAYER_STATE::STOPPED;
                    last_player_state = player_state;
                    return false;
                }
            }
            if ((millis() - last_try_timestamp_ms_ < CHECK_INTERVAL) && !playingToStopped_(player_state, last_player_state)) {
                last_player_state = player_state;
                return false;
            }

            if (measureDistance_() <= min_distance_) {
                last_player_state = player_state;
                return true;
            }
            else {
                last_player_state = player_state;
                return false;
            }
            break;
        }
        case ULTRASON_SCENARIO::PLAY_ONCE_WHEN_WITHIN: {
            // Keep playing if the player is playing
            if (player_state == PLAYER_STATE::PLAYING) {
                last_player_state = player_state;
                return true;
            }
            if ((millis() - last_try_timestamp_ms_ < CHECK_INTERVAL)) {
                last_player_state = player_state;
                return false;
            }

            if (measureDistance_() <= min_distance_) {
                last_player_state = player_state;
                return true;
            }
            else {
                last_player_state = player_state;
                return false;
            }
            break;
        }

        case ULTRASON_SCENARIO::PLAY_ONCE_WHEN_WITHIN_MORE_THAN_X_SEC_AND_AGAIN_WHEN_STILL_WITHIN_MORE_THAN_Y_SEC: {
            if (state_ == ULTRASON_STATE::INSIDE_FOR_X_SEC_AND_AGAIN_FOR_Y_SEC) {
                static uint32_t last_sucessful_try_timestamp_ms_2 = 0;
                return logicTriggerTimeThresholdInside_(player_state, last_player_state, last_sucessful_try_timestamp_ms_2);
            }
        }

        case ULTRASON_SCENARIO::PLAY_ONCE_WHEN_WITHIN_MORE_THAN_X_SEC: {
            static uint32_t last_sucessful_try_timestamp_ms = 0;
            return logicTriggerTimeThresholdInside_(player_state, last_player_state, last_sucessful_try_timestamp_ms);
            break;
        }

        case ULTRASON_SCENARIO::PLAY_ONCE_WHEN_OUTSIDE:
            return false;
            break;

        case ULTRASON_SCENARIO::PLAY_WHILE_OUTSIDE:
            return false;
            break;

        default:
            return false;
            break;
    }
}

bool Ultrason::delayReached_(uint32_t& minutes_since_act, uint8_t& seconds_since_act) {
    return (minutes_since_act * 60 + seconds_since_act < delayMin_ * 60 + delaySec_);
}

bool Ultrason::playingToStopped_(PLAYER_STATE& player_state, PLAYER_STATE& last_player_state) {
    return ((player_state == PLAYER_STATE::STOPPED) && (last_player_state == PLAYER_STATE::PLAYING));
}

bool Ultrason::logicTriggerTimeThresholdInside_(PLAYER_STATE& player_state, PLAYER_STATE& last_player_state, uint32_t& last_sucessful_try_timestamp_ms_3) {
    static uint32_t last_sucessful_try_timestamp_ms = 0;
    
    if (millis() - last_try_timestamp_ms_ < CHECK_INTERVAL) {
        last_player_state = player_state;
        return false;
    }

    logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "player_state: %d", player_state);

    if (measureDistance_() <= min_distance_) {
        if (last_player_state == PLAYER_STATE::PLAYING && player_state == PLAYER_STATE::STOPPED) {
            if (state_ == ULTRASON_STATE::INSIDE_FOR_X_SEC) {
                state_ = ULTRASON_STATE::INSIDE_FOR_X_SEC_AND_AGAIN_FOR_Y_SEC;
            }
            last_player_state = player_state;
            last_sucessful_try_timestamp_ms = millis();
            return false;
        } else if (state_ == ULTRASON_STATE::OUTSIDE) {
            state_ = ULTRASON_STATE::INSIDE_FOR_X_SEC;
        }
        last_player_state = player_state;
        return stayedInside_(last_sucessful_try_timestamp_ms);
    } else {
        if (state_ == ULTRASON_STATE::INSIDE_FOR_X_SEC_AND_AGAIN_FOR_Y_SEC) {
            state_ = ULTRASON_STATE::INSIDE_TO_OUTSIDE;
            return true;
        }
        state_ = ULTRASON_STATE::OUTSIDE;
        last_sucessful_try_timestamp_ms = millis();
        last_player_state = player_state;
        return false;
    }
}

bool Ultrason::stayedInside_(uint32_t& last_sucessful_try_timestamp_ms) {
    if (last_sucessful_try_timestamp_ms == 0) {
        last_sucessful_try_timestamp_ms = millis();
        return false;
    } else if (((millis() - last_sucessful_try_timestamp_ms >=
                time_within_minimum_sec_ * 1000) && (state_ == ULTRASON_STATE::INSIDE_FOR_X_SEC || state_ == ULTRASON_STATE::OUTSIDE)) || ((millis() - last_sucessful_try_timestamp_ms >= time_within_minimum_sec_2_ * 1000) && state_ == ULTRASON_STATE::INSIDE_FOR_X_SEC_AND_AGAIN_FOR_Y_SEC)) {
        return true;
    } else {
        return false;
    }
}

uint16_t Ultrason::measureDistance_() {
    last_try_timestamp_ms_ = millis();
    // Clears the trigPin
    digitalWrite(trig_pin_, LOW);
    delayMicroseconds(2);
    // Sets the trigPin on HIGH state for 10 micro seconds
    digitalWrite(trig_pin_, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig_pin_, LOW);

    // Reads the echoPin, returns the sound wave travel time in microseconds
    uint32_t duration = pulseIn(pin_, HIGH);
    // Calculate the distance
    float distance_cm = duration * SOUND_SPEED / 2;
    // logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "distance_cm: %f", distance_cm);
    return distance_cm;
}

void Ultrason::pickMusicSpecial_()
{
    switch(state_) {
        case ULTRASON_STATE::INSIDE_FOR_X_SEC:
            current_index_ = 0;
            break;
        case ULTRASON_STATE::INSIDE_FOR_X_SEC_AND_AGAIN_FOR_Y_SEC:
            current_index_ = 1;
            break;
        case ULTRASON_STATE::INSIDE_TO_OUTSIDE:
            current_index_ = 2;
            break;
        default:
            break;
    }
}

void Ultrason::pickMusic() {
    switch (scenario_) {
        case ULTRASON_SCENARIO::
            PLAY_ONCE_WHEN_WITHIN_MORE_THAN_X_SEC_AND_AGAIN_WHEN_STILL_WITHIN_MORE_THAN_Y_SEC:
            pickMusicSpecial_();
            break;

        default:
            Capteur::pickMusic();
            break;
    }
}
