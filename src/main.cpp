#include <Arduino.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>

#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "bouton.hpp"
#include "capteur.hpp"
#include "infrarouge.hpp"
#include "pir.hpp"
#include "ultrason.hpp"
#include "logger.hpp"

#define VERSION_CODE "2.1.2.2"

#define DELAY_FETCH 2
#define TIME_HOURS_RESTART 8
#define TIME_MINS_RESTART 0

#define CS_PIN 5
#define SD_SCK_MHZ(maxMhz) (1000000UL*(maxMhz))
#define SPI_SPEED SD_SCK_MHZ(4)

#define I2S_DIN 12
#define I2S_WCLK 27
#define I2S_BCLK 32

#define INPUT_PIN_1 17
#define OUTPUT_PIN_1 16
#define OUTPUT_PIN_2 15
#define INPUT_PIN_PLUGGED 25
#define MUTE 4

#define LOG_DIRECTORY "/.logs/"
#define MAX_LOG_SIZE 1048576  // 1MB in bytes
#define MAX_LOG_FOLDER_SIZE 10485760  // 10MB in bytes

/**************** SCENARIO AND CAPTEUR CHOICE (mandatory) ********************/
constexpr uint8_t capteurType = CAPTEUR_TYPE::PIR;
constexpr uint8_t scenario = PIR_SCENARIO::PLAY_ONCE_WHEN_MOVE;

/************************* CAN WE GO OFFLINE? *******************************/
constexpr bool is_offline = false;
constexpr bool use_battery = true;

/************************** USE INTERNAL DAC? ********************************/
constexpr bool internal_dac = false;

/******************* WAITING TRACK CONFIG (optional) *************************/
constexpr bool waiting_track = false;
constexpr uint16_t delay_before_trigger_waiting_seconds = 0;

/************************* ONLY FOR ULTRASON *******************************/
constexpr uint32_t time_within_minimum_sec = 5;
constexpr uint32_t time_within_minimum_sec_2 = 10;
constexpr uint32_t min_distance_cm = 10;

/**
 * @version 1.3.4
 * @date 01-09-2022
 *  @feature Ajout de delayFetch : le delai entre deux fetch peut être réglé en
 * minutes avec NB_SON
 *
 * @version 1.4.0
 * @date 02-09-2022
 *  @feature Ajout de delayBefore : le délai entre le moment où le mouvement est
 * détecté et le déclenchement du son peut se régler depuis le site web
 *  @feature Ajout du checking que le son est bien enregistré : si ça n'est pas
 *    le cas, le son est supprimé et sera réinstallé au prochain fetch
 *  @feature Augmentation de la mémoire : la mémoire SPIFFS est augmentée
 * à 2.5Mo aucun son ne peut être installé passé cette limite
 *  @feature Meilleure gestion aléatoire : le tirage des sons est maintenant un
 *    tirage sans remise
 *  @feature Ajout d'un checking de l'intégrité de tous les sons au démarrage
 *
 *  @version 1.4.1
 *  @date 02-09-2022
 *   @feature Suppression de certaines fonctionnalités pour gagner de la mémoire
 *
 *  @version 1.4.1.1
 *  @date 02-09-2022
 *   @feature Mise à jour de max_sound
 *
 * @version 1.4.1.2
 *  @date 02-09-2022
 *   @correctif Correction bug random
 *
 * @version 1.4.2
 *  @date 05-09-2022
 *   @correctif Correction bug fuite mémoire
 *   @correctif Optimisation mémoire
 *   @feature Ajout de la fonction deleteTooMuch
 *
 * @version 1.4.2.1
 *  @date 05-09-2022
 *   @correctif Correction prédélai
 *   @correctif Correction fetch quand un son préempte
 *
 * @version 1.4.2.2
 *  @date 06-09-2022
 *   @correctif Correction fetch écrasement de tous les sons online avant un
 * fetch
 *
 * @version 1.4.2.3
 *  @date 06-09-2022
 *   @feature Plus besoin du fingerprint
 *
 * @version 1.4.3
 *  @date 06-09-2022
 *   @feature Restart automatique à l'heure parametree
 *
 * @version 2.0
 * @name MAS-SD
 *  @date 06-09-2022
 *   @feature Ajout de la carte SD
 *
 * @version 2.1
 *  @date 03-06-2023
 *   @feature Utilisation de classes pour compatibilité capteurs
 *
 * @version 2.1.1
 *  @date 14-06-2023
 *   @feature OUTPUT_PIN_2 pullup
 *
 * @version 2.1.2
 *  @date 14-06-2023
 *   @feature scénario 3 bouton
 *
 * @version 2.1.2.1
 *  @date 14-06-2023
 *   @correction delayAfter
 *
 * @version 2.1.2.2 *
 *  @date 18-09-2023
 *   @creation version Jack
 *
 */

// Variables
int statusCode;
const char *ssid = "Default_SSID";
const char *passphrase = "Default_Password";
String st;
String content;
PLAYER_STATE player_state = STOPPED;
unsigned char currentIndex = 0;
unsigned int nbFetch = 0;

String idModule = "634f8000509b75079fa1771a7ca5ac31";
// String idModule = "f382d879def3db97acfdefeb9bc87163";

AudioGeneratorMP3 *decoder = NULL;
AudioFileSourceSD *source = NULL;
AudioOutputI2S *output = NULL;

uint32_t seconds_since_boot = 0;
uint32_t current_min_in_seconds = 0;
uint32_t seconds_since_boot_act_timestamp = 0;
uint8_t seconds = 0;
uint8_t seconds_last = 0;
uint8_t minutes = 0;
uint8_t hours = 0;
uint8_t seconds_since_act = 65;
uint32_t minutes_since_act = 10;
bool fetch = true;
bool gogogofetch = false;
bool infraredActivation = true;
unsigned char delaySecSet = 0;
unsigned char delayBefSecSet = 0;
unsigned char delayMinSet = 0;

bool was_plugged = false;
bool is_plugged = false;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Establishing Local server at port 80 whenever required
WebServer server(80);

typedef struct sound {
    String title;
    String path;
    int id = 0;
    int size = 0;
    //  int index_peer = 0;
} t_sound;

// Function Decalration
void MDCallback(void *cbData, const char *type, bool isUnicode,
                const char *string);
int     checkSoundIntegrity(t_sound toCheck, String path);
void    checkRestart();
void    checkUpdateSounds();
void    deleteTooMuch();
int     downloadAudio(t_sound soundToUpdate);
void    fetchAudiosLocal();
bool    fetchAudiosOnline();
void    handleWaitingTrack(PLAYER_STATE &player_state, uint8_t &seconds_since_act,
                 uint32_t &minutes_since_act);
void    handleTrack(PLAYER_STATE &player_state, uint8_t &seconds_since_act,
                 uint32_t &minutes_since_act);
int     removeAudio(String filename);
void    setUpTrack(const char *path);
void    updateAudios();

t_sound allSoundsOnline[NB_SON];
t_sound allSoundsStored[NB_SON];
uint8_t max_sound = 0;

Capteur *capteur;

Logger *logger;

void setup() {
    pinMode(INPUT_PIN_1, INPUT);
    pinMode(INPUT_PIN_PLUGGED, INPUT);
    pinMode(OUTPUT_PIN_1, OUTPUT);
    pinMode(OUTPUT_PIN_2, OUTPUT);
    pinMode(MUTE, OUTPUT);
    digitalWrite(MUTE, HIGH);
    Serial.begin(115200);  // Initialising if(DEBUG)Serial Monitor
    delay(10);
    is_plugged = digitalRead(INPUT_PIN_PLUGGED);
    was_plugged = is_plugged;
    // pinMode(LED_BUILTIN, OUTPUT);
    Serial.println("Starting " VERSION_CODE);

    // put your setup code here, to run once:
    Serial.begin(115200);

    if (!SD.begin()) {
        Serial.println("Card Mount Failed");
        return;
    }

    // Only activate wifi if plugged
    if (((use_battery && is_plugged) || !use_battery) && !is_offline) {
        //---------------------------------------- Read eeprom for ssid and pass
        WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP
        // it is a good practice to make sure your code sets wifi mode how you want
        // it.

        // WiFiManager, Local intialization. Once its business is done, there is no
        // need to keep it around
        WiFiManager wm;

        // Automatically connect using saved credentials,
        // if connection fails, it starts an access point with the specified name (
        // "AutoConnectAP"), if empty will auto generate SSID, if password is blank
        // it will be anonymous AP (wm.autoConnect()) then goes into a blocking loop
        // awaiting configuration and will return success result

        bool res;

        res = wm.autoConnect("AutoConnectAP", "");  // password protected ap

        if (!res) {
            Serial.println("Failed to connect");
            // ESP.restart();
        } else {
            // if you get here you have connected to the WiFi
        }
        timeClient.begin();
        timeClient.setTimeOffset(7200);
        timeClient.update();
        logger = new Logger(WiFi.getMode(), &timeClient);
        logger->printLog(__func__, LOG_LEVEL::LOG_SUCCESS, true, "Connected to WiFi");
    } else {
        WiFi.mode(WIFI_OFF);
    }

    source = new AudioFileSourceSD();
    output = new AudioOutputI2S(0,internal_dac);
    decoder = new AudioGeneratorMP3();

    if (!output->SetPinout(I2S_BCLK, I2S_WCLK, I2S_DIN)) {
        logger->printLog(__func__, LOG_LEVEL::LOG_ERROR, true, "Error setting I2S pinout");
        return;
    }

    logger->printLog(__func__, LOG_LEVEL::LOG_SUCCESS, true, "SD initialisee.");

    for (unsigned char i = 0; i < NB_SON; ++i) {
        allSoundsOnline[i].title.reserve(25);
        allSoundsStored[i].path.reserve(25);
        allSoundsStored[i].title.reserve(25);
    }

    if (capteurType == CAPTEUR_TYPE::PIR) {
        // capteur = new PIR(0, 10, D0, scenario);
        capteur = new Pir(delayMinSet, delaySecSet, INPUT_PIN_1, scenario);
    } else if (capteurType == CAPTEUR_TYPE::BOUTON) {

        // capteur = new Bouton(0, 10, OUTPUT_PIN_2, scenario);
        capteur = new Bouton(delayMinSet, delaySecSet, INPUT_PIN_1, scenario);
    } else if (capteurType == CAPTEUR_TYPE::INFRAROUGE) {
        // capteur = new Infrarouge(0, 10, D0, scenario);
        capteur = new Infrarouge(delayMinSet, delaySecSet, INPUT_PIN_1, scenario);
    }
    else if (capteurType == CAPTEUR_TYPE::ULTRASON) {
      capteur = new Ultrason(delayMinSet, delaySecSet, INPUT_PIN_1, scenario, OUTPUT_PIN_2, min_distance_cm, time_within_minimum_sec, time_within_minimum_sec_2);
    }
    else {
        logger->printLog(__func__, LOG_LEVEL::LOG_ERROR, true, "Capteur non reconnu");
        ESP.restart();
    }
    if (digitalRead(INPUT_PIN_PLUGGED)) {
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "Plugged", true);
    }
    fetchAudiosLocal();
    if (WiFi.getMode() != WIFI_OFF) {
        checkUpdateSounds();
    }
}

void loop() {
    seconds_since_boot = millis() /
              1000;  // the number of milliseconds that have passed since boot
    seconds = seconds_since_boot - current_min_in_seconds;
    if (seconds_last != seconds) {
        seconds_since_act++;
    }
    seconds_last = seconds;

    // Every minute, we add a minute to the clock.
    if (seconds >= 60) {
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "ESP.getFreeHeap(): %d", ESP.getFreeHeap());
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "Nb fetch: %d", nbFetch);
        current_min_in_seconds = seconds_since_boot;
        minutes = minutes + 1;
        fetch = true; // Every min we ask to fetch
        if (WiFi.getMode() != WIFI_OFF) {
            checkRestart();
        }
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "is_plugged: %d", digitalRead(INPUT_PIN_PLUGGED));
    }

    // the number of seconds that have passed since the last time 60 seconds was
    // reached.
    if (seconds_since_act == 60) {
        seconds_since_act = 0;
        minutes_since_act++;
    }

    if ( ((minutes % DELAY_FETCH == 0 && fetch) || (gogogofetch)) ) {
        is_plugged = digitalRead(INPUT_PIN_PLUGGED);
        if (is_plugged != was_plugged) {
            ESP.restart();
        }
        was_plugged = is_plugged;
        if (!decoder->isRunning()) {
            if (WiFi.getMode() != WIFI_OFF) {
                checkUpdateSounds();
            }
            fetch = false;
            gogogofetch = false;
        } else {
            gogogofetch = true;
        }
    }

    // if one minute has passed, start counting milliseconds from zero again and
    // add one minute to the clock.
    if (minutes >= 60) {
        minutes = 0;
        hours = hours + 1;
    }

    if (player_state == PLAYER_STATE::PLAYING) {
        digitalWrite(OUTPUT_PIN_1, HIGH);
    } else {
        digitalWrite(OUTPUT_PIN_1, LOW);
    }

    if (capteur->isTriggered(minutes_since_act, seconds_since_act, seconds_since_boot_act_timestamp,
                             seconds_since_boot, player_state)) {
        infraredActivation = false;
        if (player_state != PLAYER_STATE::PLAYING && player_state != PLAYER_STATE::PAUSED) {
            logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "Started song after delay");
            delay(delayBefSecSet * 1000);
            capteur->pickMusic();
            String pathString = '/' + allSoundsStored[capteur->getCurrentIndex()].path;
            char path[64];
            pathString.toCharArray(path, 64);
            logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "Titre: %s",
                        allSoundsStored[capteur->getCurrentIndex()].title.c_str());
            setUpTrack(path);
        }
        handleTrack(player_state, seconds_since_act, minutes_since_act);
        delay(10);
    } else if (waiting_track && ((minutes_since_act * 60 + seconds_since_act >= delay_before_trigger_waiting_seconds) || player_state == PLAYER_STATE::WAITING)) {
        if (player_state != PLAYER_STATE::WAITING) {
            logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "Started waiting track");
            setUpTrack("/waiting.mp3");
            player_state = PLAYER_STATE::WAITING;
        }
        handleWaitingTrack(player_state, seconds_since_act, minutes_since_act);
    }
}

void handleWaitingTrack(PLAYER_STATE &player_state, uint8_t &seconds_since_act,
                 uint32_t &minutes_since_act) {
    static int lastms = 0;
    if (decoder->isRunning()) {
        if (millis() - lastms > 1000) {
            lastms = millis();
            logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "Running for %d s...", lastms);
        }
        if (!decoder->loop()) decoder->stop();
    } else {
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "MP3 done");
        delay(1000);
        player_state = PLAYER_STATE::STOPPED;
    }
}

void handleTrack(PLAYER_STATE &player_state, uint8_t &seconds_since_act,
                 uint32_t &minutes_since_act) {
    static int lastms = 0;
    if (decoder->isRunning()) {
        player_state = PLAYER_STATE::PLAYING;
        if (millis() - lastms > 1000) {
            lastms = millis();
            logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "Running for %d s...", lastms);
        }
        if (!decoder->loop()) {
            decoder->stop();
        }
    } else {
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "MP3 done");
        delay(1000);
        player_state = PLAYER_STATE::STOPPED;
        seconds_since_act = 0;
        minutes_since_act = 0;
    }
}

void setUpTrack(const char *path) {
    logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "Setting up track");
    if (decoder->isRunning()) {
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "Stopping decoder");
        decoder->stop();
    }
    source->close();
    if (!source->open(path)) logger->printLog(__func__, LOG_LEVEL::LOG_ERROR, true, "Source open failed");
    decoder->begin(source, output);
}

void checkUpdateSounds() {
    if (fetchAudiosOnline()) {
        nbFetch++;
        deleteTooMuch();
        updateAudios();
    }

    for (uint8_t i = 0; i < max_sound; ++i) {
        Serial.println("----------------------------------------");
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "allSoundsOnline[%d].title: %s", i,
                 allSoundsOnline[i].title.c_str());
        Serial.println("---------");
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "allSoundsStored[%d].title: %d", i, 
                 allSoundsStored[i].title.c_str());
    }
}

void checkRestart() {
    timeClient.update();
    int currentHour = timeClient.getHours();

    int currentMinute = timeClient.getMinutes();

    logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "Time: %d:%d", currentHour, currentMinute);
    if ((currentHour == TIME_HOURS_RESTART) &&
        (currentMinute == TIME_MINS_RESTART))
        ESP.restart();
}

bool fetchAudiosOnline() {
    logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "Fetching online sounds");
    String payload;
    HTTPClient https;
    logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "[HTTPS] begin...\n");
    if (https.begin(
            "https://connect.midi-agency.com/module/tracks?id_module=" +
                idModule)) {
        // HTTP header has been send and Server response header has been handled
        int httpCode = https.GET();
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "[HTTPS] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK ||
            httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            payload = https.getString();
            Serial.println(payload);
            logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "[HTTPS] GET... payload: %s\n",
                     payload.c_str());
        } else {
            logger->printLog(__func__, LOG_LEVEL::LOG_ERROR, true, "[HTTPS] GET... failed, error: %s\n",
                     https.errorToString(httpCode).c_str());
            return false;
        }
    }
    https.end();

    for (unsigned char i = 0; i < NB_SON; ++i) {
        allSoundsOnline[i].title = "";
        allSoundsOnline[i].size = 0;
        allSoundsOnline[i].id = 0;
    }

    if (payload.indexOf("\"is_error\":false") != -1) {
        int lastIndex = 0;
        int index = 0;
        String delayMinSet_str =
            payload.substring(payload.indexOf("\"delayMin\":") + 11,
                              payload.indexOf(",\"delaySec\":", lastIndex));
        String delaySecSet_str =
            payload.substring(payload.indexOf("\"delaySec\":") + 11,
                              payload.indexOf(",\"delayBefSec\":", lastIndex));
        String delayBefSecSet_str =
            payload.substring(payload.indexOf("\"delayBefSec\":") + 14,
                              payload.indexOf(",\"data\":", lastIndex));
        delayMinSet = delayMinSet_str.toInt();
        delaySecSet = delaySecSet_str.toInt();
        delayBefSecSet = delayBefSecSet_str.toInt();
        capteur->updateDelay(delayMinSet, delaySecSet);
        while (true) {
            if (payload.indexOf("\"id\":", lastIndex + 1) != -1) {
                lastIndex = payload.indexOf("\"id\":", lastIndex + 1);

                String id = payload.substring(
                    payload.indexOf("\"id\":", lastIndex - 1) + 5,
                    payload.indexOf(",\"t\":", lastIndex));
                String title =
                    payload.substring(payload.indexOf("\"t\":", lastIndex) + 5,
                                      payload.indexOf("\",\"s\":", lastIndex));
                String size =
                    payload.substring(payload.indexOf("\"s\":", lastIndex) + 4,
                                      payload.indexOf("\"}", lastIndex));
                int idNum = id.toInt();
                int sizeNum = size.toInt();
                if (title.indexOf("\\") >= 0)
                    title = title.substring(0, title.indexOf("\\")) +
                            title.substring(title.indexOf("\\") + 1);
                allSoundsOnline[index].title = title;
                allSoundsOnline[index].size = sizeNum;
                allSoundsOnline[index++].id = idNum;
            } else
                break;
        }
        Serial.println("done fetching");
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "done fetching");
    } else {
        logger->printLog(__func__, LOG_LEVEL::LOG_ERROR, true, "is_error = true");
        return false;
    }
    return true;
}

void fetchAudiosLocal() {
    File dir = SD.open("/");
    int index = 0;
    // bool first = true;
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) {
            // no more files
            break;
        }
        if (strcmp(entry.name(), "System Volume Information") == 0) continue;
        if (entry.name()[0] == '.') continue;
        if (strcmp(entry.name(), "waiting.mp3") == 0) continue;
        if (strcmp(entry.name(), "logs") == 0) continue;
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "%s \t %d", entry.name(), entry.size());

        allSoundsStored[index].path = entry.name();
        allSoundsStored[index].size = entry.size();
        allSoundsStored[index++].title = entry.name();

        entry.close();
    }
    max_sound = index;
    capteur->setMaxSound(max_sound);
}

void deleteTooMuch() {
    t_sound newAllSoundStored[NB_SON];
    int index = 0;
    for (int stored = 0; stored < NB_SON; ++stored) {
        bool toRemove = true;
        for (int online = 0; online < NB_SON; ++online) {
            if (allSoundsOnline[online].title ==
                allSoundsStored[stored].title) {
                newAllSoundStored[index] =
                    allSoundsStored[stored];  // On copie l'ancien dans le nouveau
                newAllSoundStored[index++].id =
                    allSoundsOnline[online].id;  // On rajoute l'id
                toRemove = false;
                break;
            }
        }
        if (toRemove) {
            logger->printLog(__func__, LOG_LEVEL::LOG_WARNING, false, "to remove: %s",
                     allSoundsStored[stored].title.c_str());
            removeAudio('/' + allSoundsStored[stored].path);
        }
    }
    for (unsigned int i = 0; i < NB_SON; ++i)
        allSoundsStored[i] = newAllSoundStored[i];
}

void updateAudios() {
    t_sound newAllSoundStored[NB_SON];
    unsigned char index = 0;
    // Checking des sons à update
    for (unsigned char online = 0; online < NB_SON; ++online) {
        bool toUpdate = true;
        if (allSoundsOnline[online].title == "") break;
        for (unsigned char stored = 0; stored < NB_SON; ++stored) {
            // Si dans les fichiers locaux, on a déjà le fichier online
            // correspondant, on copie les info si nécessaire et on passe.
            // Sinon, on lève un flag 'toUpdate'
            if (allSoundsOnline[online].title ==
                allSoundsStored[stored].title) {
                newAllSoundStored[index] =
                    allSoundsStored[stored];  // On copie l'ancien dans le
                                              // nouveau
                newAllSoundStored[index++].id =
                    allSoundsOnline[online].id;  // On rajoute l'id
                toUpdate = false;
                break;
            }
        }
        // Si il faut downloader le son en ligne pour l'avoir en local
        if (toUpdate) {
            Serial.println("--------- TOUPDATE ---------");
            Serial.println("allSoundsOnline[online]");
            Serial.println(allSoundsOnline[online].id);
            Serial.println(allSoundsOnline[online].title);
            Serial.println(allSoundsOnline[online].path);
            // On télécharge le son, puis on update les différentes infos
            if (downloadAudio(allSoundsOnline[online]) == 0) {
                if (checkSoundIntegrity(allSoundsOnline[online],
                                        "/" + allSoundsOnline[online].title) ==
                    0) {
                    newAllSoundStored[index].id = allSoundsOnline[online].id;
                    newAllSoundStored[index].size =
                        allSoundsOnline[online].size;
                    newAllSoundStored[index].path =
                        "/" + allSoundsOnline[online].title;
                    newAllSoundStored[index++].title =
                        allSoundsOnline[online].title;
                    logger->printLog(__func__, LOG_LEVEL::LOG_SUCCESS, true, "Audio %s properly installed", allSoundsOnline[online].title);
                } else {
                    logger->printLog(__func__, LOG_LEVEL::LOG_ERROR, true, "Audio %s was deleted because not complete",
                             allSoundsOnline[online].title);
                }
            } else
                logger->printLog(__func__, LOG_LEVEL::LOG_ERROR, true, "Audio %s could not be downloaded",
                         allSoundsOnline[online].title);
        } else {
            checkSoundIntegrity(allSoundsOnline[online],
                                         "/" + allSoundsOnline[online].title);
        }
    }
    // Enfin, il faut supprimer les sons qui sont en trop en comparant
    // l'ancienne liste et la nouvelle. Si on trouve un son de l'ancienne liste
    // dans la nouvelle, on passe. Sinon, c'est que ce son n'existe plus dans la
    // nouvelle liste, et il faut le supprimer.
    for (unsigned char previous = 0; previous < NB_SON; ++previous) {
        bool stillThere = false;
        for (unsigned char newList = 0; newList < NB_SON; ++newList) {
            if (allSoundsStored[previous].title ==
                newAllSoundStored[newList].title) {
                stillThere = true;
                break;
            }
        }
        if (!stillThere) removeAudio(allSoundsStored[previous].path);
    }

    for (unsigned char i = 0; i < NB_SON; ++i)
        allSoundsStored[i] = newAllSoundStored[i];
    max_sound = index;
    capteur->setMaxSound(max_sound);
}

int checkSoundIntegrity(t_sound toCheck, String path) {
    Serial.print("path: ");
    Serial.println(path);
    File f = SD.open(path, FILE_READ);
    int sizeFile = f.size();
    f.close();
    Serial.print("sizeFile: ");
    Serial.println(sizeFile);
    Serial.print("toCheck.size: ");
    Serial.println(toCheck.size);
    if (sizeFile == toCheck.size)
        return 0;
    else {
        if (SD.remove(path)) {
            logger->printLog(__func__, LOG_LEVEL::LOG_WARNING, true, "Son incomplet supprimé avec succès");
            return -1;
        } else {
            logger->printLog(__func__, LOG_LEVEL::LOG_ERROR, true, "Erreur lors de la suppression du son incomplet");
            return -2;
        }
    }
}

int removeAudio(String filename) {
    File dataFile = SD.open(filename, FILE_READ);
    if (dataFile) {
        if (SD.remove(filename)) {
            Serial.println("Son supprimé avec succès");
        } else {
            logger->printLog(__func__, LOG_LEVEL::LOG_ERROR, true, "Erreur lors de la suppression du son");
            return -1;
        }
    } else {
        logger->printLog(__func__, LOG_LEVEL::LOG_WARNING, true, "Son absent");
        return -1;
    }
    dataFile.close();
    return 0;
}

int downloadAudio(t_sound soundToUpdate) {
    HTTPClient https;
    String URL = "https://connect.midi-agency.com/module/track/path?id=";
    URL += soundToUpdate.id;
    https.begin(URL);
    int httpCode = https.GET();

    String path_brute;
    logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "URL: %s", URL.c_str());

    // file found at server
    if (httpCode == HTTP_CODE_OK ||
        httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        path_brute = https.getString();
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "HTTP Code: %d", httpCode);
    } else {
        logger->printLog(__func__, LOG_LEVEL::LOG_ERROR, true, "[HTTPS] GET... failed, error: %s\n",
                 https.errorToString(httpCode).c_str());
    }
    https.end();

    path_brute = path_brute.substring(1, path_brute.indexOf("\"", 5));
    path_brute.replace("\\", "");

    if (https.begin("https://connect.midi-agency.com/" + path_brute)) {
        // start connection and send HTTP header
        int httpCode = https.GET();
        logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "HTTP Code: %d", httpCode);
        // httpCode will be negative on error
        if (httpCode > 0) {
            String audioName = '/' + soundToUpdate.title;
            logger->printLog(__func__, LOG_LEVEL::LOG_INFO, true, "audioName: %s", audioName.c_str());
            File f = SD.open(audioName, FILE_WRITE);
            // read all data from server
            if (f) {
                int len = https.getSize();
                // get tcp stream
                WiFiClient * stream = https.getStreamPtr();
                int nbBytesPacket = 0;
                bool blink = false;
                uint8_t timeout_count = 0;
                uint8_t buff[128] = {0};
                // client->setTimeout(100);
                while (https.connected() && (len > 0 || len == -1)) {
                    size_t size = stream->available();

                    if (size) {  
                        // read up to 256 bytes
                        int c = stream->readBytes(
                            buff, std::min((size_t)len, sizeof(buff)));

                        f.write(buff, c);
                        if (++nbBytesPacket % 100 == 0) {
                            logger->printLog(__func__, LOG_LEVEL::LOG_INFO, false, "NE PAS DEBRANCHER\n\tnbBytesPacket: %d\n",
                                        nbBytesPacket);
                        }

                        if (nbBytesPacket % 10 == 0) {
                            // digitalWrite(LED_BUILTIN, blink);
                            blink = !blink;
                        }

                        timeout_count = 0;
                        if (len > 0) {
                            len -= c;
                        }
                    } else {
                        timeout_count++;
                        if (timeout_count > 500) {
                            logger->printLog(__func__, LOG_LEVEL::LOG_ERROR, true, "Read timeout too many times");
                            f.close();
                            https.end();
                            return 0;
                        }
                    }
                    delay(1);
                }
                logger->printLog(__func__, LOG_LEVEL::LOG_SUCCESS, true, "Audio downloaded");
                f.close();
            } else
                return -1;
        } else
            return -1;
    } else
        return -1;
    https.end();
    return 0;
}