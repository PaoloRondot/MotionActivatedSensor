#include "capteur.hpp"

extern void printLog(const char* function, LOG_LEVEL level, const char* message, ...);

Capteur::Capteur(const int& delayMin, const int& delaySec, const uint8_t& pin, const int& scenario)
:delayMin_(delayMin), delaySec_(delaySec), pin_(pin), scenario_(scenario)
{
}

void Capteur::setMaxSound(const uint8_t& max_sound) {
    printLog(__func__, LOG_INFO, "start setMaxSound() max_sound: %d", max_sound);
    max_sound_ = max_sound;
    if (max_sound_ > 1) {
        randomizeAll_();
    }
}

void Capteur::randomizeAll_() { 
    printLog(__func__, LOG_INFO, "start randomizeAll_()");
    for (uint8_t i = 0; i < max_sound_; i++) {  // fill array
        randomized_indexes_[i] = i;
        Serial.printf("%d,", randomized_indexes_[i]);
    }

    for (uint8_t i = 0; i < max_sound_; i++) {  // shuffle array
        uint8_t temp = randomized_indexes_[i];
        uint8_t randomIndex = rand() % max_sound_;

        randomized_indexes_[i] = randomized_indexes_[randomIndex];
        randomized_indexes_[randomIndex] = temp;
    }
    printLog(__func__, LOG_INFO, "randomized_indexes_ : ");
    for (uint8_t i = 0; i < max_sound_; i++) {  // print array
        Serial.printf("%d,", randomized_indexes_[i]);
    }
    Serial.println("");
}

void Capteur::pickMusic() {
    printLog(__func__, LOG_INFO, "_max_sound: %d", max_sound_);
    if (current_index_ == max_sound_ - 1) {
        current_index_ = 0;
        if (max_sound_ > 1) {
            randomizeAll_();
        }
    }
    else {
        current_index_ ++;
    }
    printLog(__func__, LOG_INFO, "current_index_: %d", current_index_);
}

void Capteur::updateDelay(const int& delayMin, const int& delaySec) {
    delayMin_ = delayMin;
    delaySec_ = delaySec;
}
