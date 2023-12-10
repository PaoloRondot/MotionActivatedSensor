#include "ultrason.hpp"

#define SOUND_SPEED 0.034

extern void printLog(const char* function, LOG_LEVEL level, const char* message, ...);

Ultrason::Ultrason(const int& delayMin, const int& delaySec, const uint8_t& echo_pin, const int& scenario, const uint8_t& trig_pin, const uint16_t& min_distance)
:Capteur(delayMin, delaySec, echo_pin, scenario), trig_pin_(trig_pin), min_distance_(min_distance)
{
}

Ultrason::~Ultrason()
{
}

bool Ultrason::isTriggered(uint32_t &minutes_since_act, uint8_t &seconds_since_act, uint32_t seconds_since_boot_act_timestamp, const uint32_t &seconds_since_boot, PLAYER_STATE &player_state) {
    static uint32_t last_try_timestamp_ms = 0;
    switch (scenario_)
    {
    case ULTRASON_SCENARIO::PLAY_ONCE_WHEN_WITHIN:
    {
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
        
        // Reads the echoPin, returns the sound wave travel time in microseconds
        uint32_t duration = pulseIn(pin_, HIGH);
        printLog(__func__, LOG_LEVEL::LOG_INFO, "duration: %d", duration);
        // Calculate the distance
        float distance_cm = duration * SOUND_SPEED/2;
        printLog(__func__, LOG_LEVEL::LOG_INFO, "distance_cm: %f", distance_cm);
        if (distance_cm <= min_distance_)
            return true;
        else {
            return false;
        }
        break;
    }

    case ULTRASON_SCENARIO::PLAY_WHILE_WITHIN:
        return false;
        break;

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