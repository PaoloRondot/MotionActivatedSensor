#include <Arduino.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
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

#define VERSION_CODE "2.1.2.2"

#define DELAY_FETCH 2
#define TIME_HOURS_RESTART 8
#define TIME_MINS_RESTART 0

#define CS_PIN D1
#define SPI_SPEED SD_SCK_MHZ(20)

/**************** SCENARIO AND CAPTEUR CHOICE (mandatory) ********************/
constexpr uint8_t capteurType = CAPTEUR_TYPE::PIR;
constexpr uint8_t scenario = PIR_SCENARIO::PLAY_ONCE_WHEN_MOVE;

/************************* CAN WE GO OFFLINE? *******************************/
constexpr bool is_offline = false;

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
 *   @feature D3 pullup
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

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Establishing Local server at port 80 whenever required
ESP8266WebServer server(80);

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
int     downloadAudio(t_sound soundToUpdade);
void    fetchAudiosLocal();
void    fetchAudiosOnline();
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

void printLog(const char* function, LOG_LEVEL level, const char* message, ...) {
    if (level == LOG_INFO) {
        Serial.print("[INFO] ");
    } else if (level == LOG_WARNING) {
        Serial.print("[WARNING] ");
    } else if (level == LOG_ERROR) {
        Serial.print("[ERROR] ");
    }
    Serial.print(function);
    Serial.print(" : ");

    va_list args;
    va_start(args, message);
    char buffer[256];
    vsnprintf(buffer, 256, message, args);
    va_end(args);
    Serial.println(buffer);
}

void setup() {
    pinMode(D2, OUTPUT);
    pinMode(D0, INPUT);
    pinMode(D3, OUTPUT);
    Serial.begin(115200);  // Initialising if(DEBUG)Serial Monitor
    delay(10);
    pinMode(LED_BUILTIN, OUTPUT);
    printLog(__func__, LOG_INFO, "Starting " VERSION_CODE);

    //---------------------------------------- Read eeprom for ssid and pass
    WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want
    // it.

    // put your setup code here, to run once:
    Serial.begin(115200);

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
        printLog(__func__, LOG_ERROR, "Failed to connect");
        // ESP.restart();
    } else {
        // if you get here you have connected to the WiFi
        printLog(__func__, LOG_INFO, "Connected");
    }

    audioLogger = &Serial;

    source = new AudioFileSourceSD();
    output = new AudioOutputI2S();
    decoder = new AudioGeneratorMP3();

    if (!SD.begin(CS_PIN, SPI_SPEED)) {
        printLog(__func__, LOG_ERROR, "Probleme carte SD");
        return;
    }
    printLog(__func__, LOG_INFO, "SD initialisee.");

    for (unsigned char i = 0; i < NB_SON; ++i) {
        allSoundsOnline[i].title.reserve(25);
        allSoundsStored[i].path.reserve(25);
        allSoundsStored[i].title.reserve(25);
    }

    if (capteurType == CAPTEUR_TYPE::PIR) {
        // capteur = new PIR(0, 10, D0, scenario);
        capteur = new Pir(delayMinSet, delaySecSet, D0, scenario);
    } else if (capteurType == CAPTEUR_TYPE::BOUTON) {

        // capteur = new Bouton(0, 10, D3, scenario);
        capteur = new Bouton(delayMinSet, delaySecSet, D0, scenario);
    } else if (capteurType == CAPTEUR_TYPE::INFRAROUGE) {
        // capteur = new Infrarouge(0, 10, D0, scenario);
        capteur = new Infrarouge(delayMinSet, delaySecSet, D0, scenario);
    }
    else if (capteurType == CAPTEUR_TYPE::ULTRASON) {
      capteur = new Ultrason(delayMinSet, delaySecSet, D0, scenario, D3, min_distance_cm, time_within_minimum_sec, time_within_minimum_sec_2);
    }
    else {
        printLog(__func__, LOG_ERROR, "Capteur non reconnu");
        ESP.restart();
    }

    fetchAudiosLocal();
    if (!is_offline) {
        checkUpdateSounds();
    }

    timeClient.begin();
    timeClient.setTimeOffset(7200);
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
        printLog(__func__, LOG_INFO, "ESP.getFreeHeap(): %d", ESP.getFreeHeap());
        printLog(__func__, LOG_INFO, "Nb fetch: %d", nbFetch);
        current_min_in_seconds = seconds_since_boot;
        minutes = minutes + 1;
        fetch = true; // Every min we ask to fetch
        // checkRestart();
    }

    // the number of seconds that have passed since the last time 60 seconds was
    // reached.
    if (seconds_since_act == 60) {
        seconds_since_act = 0;
        minutes_since_act++;
    }

    if ((minutes % DELAY_FETCH == 0 && fetch) || (gogogofetch)) {
        if (!decoder->isRunning()) {
            if (!is_offline) {
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
        digitalWrite(D2, HIGH);
    } else {
        digitalWrite(D2, LOW);
    }

    if (capteur->isTriggered(minutes_since_act, seconds_since_act, seconds_since_boot_act_timestamp,
                             seconds_since_boot, player_state)) {
        infraredActivation = false;
        if (player_state != PLAYER_STATE::PLAYING && player_state != PLAYER_STATE::PAUSED) {
            printLog(__func__, LOG_INFO, "Started song after delay");
            delay(delayBefSecSet * 1000);
            capteur->pickMusic();
            String pathString = allSoundsStored[capteur->getCurrentIndex()].path;
            char path[64];
            pathString.toCharArray(path, 64);
            printLog(__func__, LOG_INFO, "Titre: %s",
                        allSoundsStored[capteur->getCurrentIndex()].title.c_str());
            setUpTrack(path);
        }
        handleTrack(player_state, seconds_since_act, minutes_since_act);
    } else if (waiting_track && ((minutes_since_act * 60 + seconds_since_act >= delay_before_trigger_waiting_seconds) || player_state == PLAYER_STATE::WAITING)) {
        if (player_state != PLAYER_STATE::WAITING) {
            printLog(__func__, LOG_INFO, "Waiting...");
            setUpTrack("/waiting");
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
            printLog(__func__, LOG_INFO, "Running for %d s...", lastms);
        }
        if (!decoder->loop()) decoder->stop();
    } else {
        printLog(__func__, LOG_INFO, "MP3 done");
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
            printLog(__func__, LOG_INFO, "Running for %d ms...", lastms);
        }
        if (!decoder->loop()) decoder->stop();
    } else {
        printLog(__func__, LOG_INFO, "MP3 done");
        delay(1000);
        player_state = PLAYER_STATE::STOPPED;
        seconds_since_act = 0;
        minutes_since_act = 0;
    }
}

void setUpTrack(const char *path) {
    if (decoder->isRunning()) {
        decoder->stop();
    }
    source->close();
    source->open(path);
    decoder->begin(source, output);
}

void checkUpdateSounds() {
    fetchAudiosOnline();
    nbFetch++;
    deleteTooMuch();

    updateAudios();
    for (uint8_t i = 0; i < max_sound; ++i) {
        Serial.println(F("----------------------------------------"));
        printLog(__func__, LOG_INFO, "allSoundsOnline[%d].title: %s", i,
                 allSoundsOnline[i].title.c_str());
        Serial.println("---------");
        printLog(__func__, LOG_INFO, "allSoundsStored[%d].id: %d", i,
                 allSoundsStored[i].title.c_str());
    }
    Serial.print("ESP.getFreeHeap(): ");
    Serial.println(ESP.getFreeHeap());
}

void checkRestart() {
    timeClient.update();
    int currentHour = timeClient.getHours();
    Serial.print("Hour: ");
    Serial.println(currentHour);

    int currentMinute = timeClient.getMinutes();
    Serial.print("Minutes: ");
    Serial.println(currentMinute);
    if ((currentHour == TIME_HOURS_RESTART) &&
        (currentMinute == TIME_MINS_RESTART))
        ESP.restart();
}

void fetchAudiosOnline() {
    printLog(__func__, LOG_INFO, "Fetching online sounds");
    for (unsigned char i = 0; i < NB_SON; ++i) {
        allSoundsOnline[i].title = "";
        allSoundsOnline[i].size = 0;
        allSoundsOnline[i].id = 0;
    }
    String payload;
    std::unique_ptr<BearSSL::WiFiClientSecure> client(
        new BearSSL::WiFiClientSecure);
    client->setInsecure();
    HTTPClient https;
    printLog(__func__, LOG_INFO, "[HTTPS] begin...\n");
    if (https.begin(
            *client,
            F("https://connect.midi-agency.com/module/tracks?id_module=") +
                idModule)) {
        // HTTP header has been send and Server response header has been handled
        int httpCode = https.GET();
        printLog(__func__, LOG_INFO, "[HTTPS] GET... code: " + httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK ||
            httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            payload = https.getString();
            Serial.println(payload);
        } else {
            Serial.printf("[HTTPS] GET... failed, error: %s\n",
                          https.errorToString(httpCode).c_str());
        }
    }
    https.end();
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
    } else
        Serial.println(F("is_error = true"));
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
        if (strcmp(entry.name(), "waiting") == 0) continue;
        printLog(__func__, LOG_INFO, "%s \t %d", entry.name(), entry.size());

        allSoundsStored[index].path = entry.fullName();
        allSoundsStored[index].size = entry.size();
        allSoundsStored[index++].title = entry.name();

        entry.close();
    }
    max_sound = index - 1;
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
                    allSoundsStored[stored];  // On copie l'ancien dans le
                                              // nouveau
                newAllSoundStored[index++].id =
                    allSoundsOnline[online].id;  // On rajoute l'id
                toRemove = false;
                break;
            }
        }
        if (toRemove) {
            Serial.print("to remove: ");
            Serial.println(allSoundsStored[stored].title);
            removeAudio(allSoundsStored[stored].path);
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
            Serial.println(F("--------- TOUPDATE ---------"));
            Serial.println(F("allSoundsOnline[online]"));
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
                    Serial.println(F("Audio properly installed"));
                } else {
                    Serial.println(F("Audio was deleted because not complete"));
                }
            } else
                Serial.println(F("Audio could not be downloaded"));
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
            Serial.println(F("Son incomplet supprimé avec succès"));
            return -1;
        } else {
            Serial.println(F("Erreur lors de la suppression du son incomplet"));
            return -2;
        }
    }
}

int removeAudio(String filename) {
    File dataFile = SD.open(filename, FILE_READ);
    if (dataFile) {
        if (SD.remove(filename)) {
            Serial.println(F("Son supprimé avec succès"));
        } else {
            Serial.println(F("Erreur lors de la suppression du son"));
            return -1;
        }
    } else {
        Serial.println(F("Son non présent"));
        return -1;
    }
    dataFile.close();
    return 0;
}

int downloadAudio(t_sound soundToUpdade) {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(
        new BearSSL::WiFiClientSecure);
    client->setInsecure();
    HTTPClient https;
    String path_brute;
    String URL = "https://connect.midi-agency.com/module/track/path?id=";
    URL += soundToUpdade.id;
    Serial.print("URL: ");
    Serial.println(URL);
    if (https.begin(*client, URL)) {
        // HTTP header has been send and Server response header has been handled
        int httpCode = https.GET();

        // file found at server
        if (httpCode == HTTP_CODE_OK ||
            httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
            path_brute = https.getString();
        } else {
            Serial.printf("[HTTPS] GET... failed, error: %s\n",
                          https.errorToString(httpCode).c_str());
        }
    }
    https.end();

    path_brute = path_brute.substring(1, path_brute.indexOf("\"", 5));
    path_brute.replace("\\", "");
    Serial.println(path_brute);

    // String URL = F("https://connect.midi-agency.com/");
    // URL += soundToUpdade.path;
    // Serial.println(soundToUpdade.path);
    Serial.print(F("[HTTPS] begin...\n"));
    if (https.begin(*client,
                    F("https://connect.midi-agency.com/") + path_brute)) {
        Serial.println(F("[HTTPS] GET...\n"));
        // start connection and send HTTP header
        int httpCode = https.GET();
        Serial.print(F("HTTP Code: "));
        Serial.println(httpCode);
        // httpCode will be negative on error
        if (httpCode > 0) {
            String audioName = soundToUpdade.title;
            // if (audioName.indexOf("\\") >= 0)
            //   audioName = audioName.substring(0,audioName.indexOf("\\")) +
            //   audioName.substring(audioName.indexOf("\\")+1);
            // soundToUpdade.title = audioName;
            Serial.print(F("audioName: "));
            Serial.println(audioName);
            File f = SD.open(audioName, FILE_WRITE);
            // read all data from server
            if (f) {
                int len = https.getSize();
                uint8_t buff[128] = {0};
                int nbBytes = 0;
                bool blink = false;
                while (https.connected() && (len > 0 || len == -1)) {
                    // read up to 128 byte
                    int c = client->readBytes(
                        buff, std::min((size_t)len, sizeof(buff)));
                    f.write(buff, c);
                    if (++nbBytes % 100 == 0)
                        Serial.printf("NE PAS DEBRANCHER\n\tnbBytes: %d\n",
                                      nbBytes);

                    if (nbBytes % 10 == 0) {
                        digitalWrite(LED_BUILTIN, blink);
                        blink = !blink;
                    }
                    if (!c) {
                        Serial.println(F("read timeout"));
                    }

                    if (len > 0) {
                        len -= c;
                    }
                }
                Serial.println("done");
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