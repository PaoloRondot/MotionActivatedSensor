#include "ultrason.hpp"

constexpr float SOUND_SPEED = 0.034;

extern void printLog(const char* function, LOG_LEVEL level, const char* message,
                     ...);

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
    
    static uint32_t last_try_timestamp_ms = 0;
    
    switch (scenario_) {
        case ULTRASON_SCENARIO::PLAY_ONCE_WHEN_WITHIN: {
            if (player_state == PLAYER_STATE::PLAYING) {
                return true;
            }
            if (millis() - last_try_timestamp_ms < 100) {
                return false;
            }
            last_try_timestamp_ms = millis();
            // Clears the trigPin
            digitalWrite(trig_pin_, LOW);
            delayMicroseconds(2);
            // Sets the trigPin on HIGH state for 10 micro seconds
            digitalWrite(trig_pin_, HIGH);
            delayMicroseconds(10);
            digitalWrite(trig_pin_, LOW);

            // Reads the echoPin, returns the sound wave travel time in
            // microseconds
            uint32_t duration = pulseIn(pin_, HIGH);
            printLog(__func__, LOG_LEVEL::LOG_INFO, "duration: %d", duration);
            // Calculate the distance
            float distance_cm = duration * SOUND_SPEED / 2;
            printLog(__func__, LOG_LEVEL::LOG_INFO, "distance_cm: %f",
                     distance_cm);
            if (distance_cm <= min_distance_)
                return true;
            else {
                return false;
            }
            break;
        }

        case ULTRASON_SCENARIO::PLAY_ONCE_WHEN_WITHIN_MORE_THAN_X_SEC_AND_AGAIN_WHEN_STILL_WITHIN_MORE_THAN_Y_SEC: {
            if (state_ == ULTRASON_STATE::INSIDE_FOR_X_SEC_AND_AGAIN_FOR_Y_SEC) {
                static uint32_t last_sucessful_try_timestamp_ms_2 = 0;
                return logicTriggerTimeThresholdInside_(player_state,
                                                    last_try_timestamp_ms, last_sucessful_try_timestamp_ms_2);
            }
        }

        case ULTRASON_SCENARIO::PLAY_ONCE_WHEN_WITHIN_MORE_THAN_X_SEC: {
            static uint32_t last_sucessful_try_timestamp_ms = 0;
            return logicTriggerTimeThresholdInside_(player_state,
                                                    last_try_timestamp_ms, last_sucessful_try_timestamp_ms);
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

bool Ultrason::logicTriggerTimeThresholdInside_(PLAYER_STATE& player_state, uint32_t& last_try_timestamp_ms, uint32_t& last_sucessful_try_timestamp_ms) {
    static PLAYER_STATE last_player_state = PLAYER_STATE::STOPPED;
    if (player_state == PLAYER_STATE::PLAYING) {
        last_player_state = player_state;
        return true;
    } else if (millis() - last_try_timestamp_ms < 100) {
        last_player_state = player_state;
        return false;
    }

    last_try_timestamp_ms = millis();
    printLog(__func__, LOG_LEVEL::LOG_INFO, "state_: %d", state_);
    if (measureDistance_() <= min_distance_) {
        if (last_player_state == PLAYER_STATE::PLAYING && player_state == PLAYER_STATE::STOPPED && state_ == ULTRASON_STATE::INSIDE_FOR_X_SEC) {
            state_ = ULTRASON_STATE::INSIDE_FOR_X_SEC_AND_AGAIN_FOR_Y_SEC;
            last_player_state = player_state;
            last_sucessful_try_timestamp_ms = 0;
            return false;
        } else if (state_ == ULTRASON_STATE::OUTSIDE) {
            state_ = ULTRASON_STATE::INSIDE_FOR_X_SEC;
        }
        last_player_state = player_state;
        return stayedInside_(last_sucessful_try_timestamp_ms);
    } else {
        if (player_state == PLAYER_STATE::STOPPED && last_player_state == PLAYER_STATE::PLAYING && (state_ == ULTRASON_STATE::INSIDE_FOR_X_SEC || state_ == ULTRASON_STATE::INSIDE_FOR_X_SEC_AND_AGAIN_FOR_Y_SEC)) {
            state_ = ULTRASON_STATE::INSIDE_TO_OUTSIDE;
            return true;
        }
        state_ = ULTRASON_STATE::OUTSIDE;
        last_sucessful_try_timestamp_ms = 0;
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
    printLog(__func__, LOG_LEVEL::LOG_INFO, "distance_cm: %f", distance_cm);
    return distance_cm;
}

void Ultrason::pickMusicSpecial_()
{
    switch(state_) {
        case ULTRASON_STATE::INSIDE_FOR_X_SEC:
            current_index_ = 2;
            break;
        case ULTRASON_STATE::INSIDE_FOR_X_SEC_AND_AGAIN_FOR_Y_SEC:
            printLog(__func__, LOG_LEVEL::LOG_INFO, "state_ == ULTRASON_STATE::INSIDE_FOR_X_SEC_AND_AGAIN_FOR_Y_SEC");
            current_index_ = 0;
            break;
        case ULTRASON_STATE::INSIDE_TO_OUTSIDE:
            printLog(__func__, LOG_LEVEL::LOG_INFO, "state_ == ULTRASON_STATE::INSIDE_TO_OUTSIDE");
            current_index_ = 1;
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
