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

#define VERSION "2.1.2.2"

#define NB_SON 25
#define DELAY_FETCH 2
#define TIME_HOURS_RESTART 8
#define TIME_MINS_RESTART 0

#define CS_PIN D1
#define SPI_SPEED SD_SCK_MHZ(20)

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
bool letsgo = false;
unsigned char allIndexes[NB_SON];
unsigned char currentIndex = 0;
unsigned int nbFetch = 0;

String idModule = "634f8000509b75079fa1771a7ca5ac31";

AudioGeneratorMP3 *decoder = NULL;
AudioFileSourceSD *source = NULL;
AudioOutputI2S *output = NULL;

unsigned long timeNow = 0;
unsigned long timeLast = 0;
unsigned long timeLast2 = 0;
unsigned char seconds = 0;
unsigned char prevSeconds = 0;
unsigned char minutes = 0;
unsigned char minutes_last = 0;
int hours = 0;
unsigned long delaySinceActSec = 1000;
unsigned long delaySinceActMin = 1000;
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
// void initA(char *filename);
void updateAudios();
void fetchAudiosOnline();
void fetchAudiosLocal();
void deleteTooMuch();
void updateAudios();
int downloadAudio(t_sound soundToUpdade);
int checkSoundIntegrity(t_sound toCheck, String path);
int removeAudio(String filename);
void randomizeAll();
void checkRestart();

t_sound allSoundsOnline[NB_SON];
t_sound allSoundsStored[NB_SON];
unsigned char max_sound = 0;

#define CAP_PIR 0
#define CAP_BOU 1
#define CAP_INR 2
#define CAP_ULT 3

int capteurType = CAP_BOU;
int scenario = BOUTON_SCENARIO::PLAY_WHEN_PRESSED_AND_RESTART;

Capteur *capteur;

void checkUpdateSounds() {
    fetchAudiosOnline();
    nbFetch++;
    // Serial.print("fetch audios online ESP.getFreeHeap(): ");
    // Serial.println(ESP.getFreeHeap());
    deleteTooMuch();
    // Serial.print("delete too much ESP.getFreeHeap(): ");
    // Serial.println(ESP.getFreeHeap());

    updateAudios();
    // Serial.print("update audio ESP.getFreeHeap(): ");
    // Serial.println(ESP.getFreeHeap());
    for (unsigned char i = 0; i < max_sound; ++i) {
        Serial.println(F("----------------------------------------"));
        Serial.print(F("allSoundsOnline[i].title: "));
        Serial.println(allSoundsOnline[i].title);
        // Serial.print(F("allSoundsOnline[i].size: "));
        // Serial.println(allSoundsOnline[i].size);
        Serial.println("---------");
        Serial.print(F("allSoundsStored[i].title: "));
        Serial.println(allSoundsStored[i].title);
        // Serial.print(F("allSoundsStored[i].size: "));
        // Serial.println(allSoundsStored[i].size);
    }
    Serial.print("ESP.getFreeHeap(): ");
    Serial.println(ESP.getFreeHeap());
}

void setup() {
    pinMode(D2, OUTPUT);
    pinMode(D0, INPUT);
    Serial.begin(115200);  // Initialising if(DEBUG)Serial Monitor
    delay(10);
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.print("Version ");
    Serial.println(VERSION);
    Serial.println();
    Serial.println();
    Serial.println("Startup");

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
        Serial.println(F("Failed to connect"));
        // ESP.restart();
    } else {
        // if you get here you have connected to the WiFi
        Serial.println(F("connected...yeey :)"));
    }

    audioLogger = &Serial;

    source = new AudioFileSourceSD();
    output = new AudioOutputI2S();
    decoder = new AudioGeneratorMP3();

    if (!SD.begin(CS_PIN, SPI_SPEED)) {
        Serial.println("Probleme carte SD");
        return;
    }
    Serial.println("SD initialisee.");

    for (unsigned char i = 0; i < NB_SON; ++i) {
        allSoundsOnline[i].title.reserve(25);
        allSoundsStored[i].path.reserve(25);
        allSoundsStored[i].title.reserve(25);
    }

    fetchAudiosLocal();
    checkUpdateSounds();
    randomizeAll();

    timeClient.begin();
    timeClient.setTimeOffset(7200);

    if (capteurType == CAP_PIR) {
        // capteur = new PIR(0, 10, D0, scenario);
        capteur = new PIR(delayMinSet, delaySecSet, D0, scenario);
    } else if (capteurType == CAP_BOU) {
        // capteur = new Bouton(0, 10, D3, scenario);
        capteur = new Bouton(delayMinSet, delaySecSet, D0, scenario);
    } else if (capteurType == CAP_INR) {
        // capteur = new Infrarouge(0, 10, D0, scenario);
        capteur = new Infrarouge(delayMinSet, delaySecSet, D0, scenario);
    }
    // else if (capteurType == CAP_ULT) {
    //   capteur = new Ultrason(D0, scenario);
    // }
    else {
        Serial.println(F("Capteur non reconnu"));
        ESP.restart();
    }
}

void loop() {
    timeNow = millis() /
              1000;  // the number of milliseconds that have passed since boot
    delaySinceActSec = timeNow - timeLast2;
    seconds = timeNow - timeLast;

    // the number of seconds that have passed since the last time 60 seconds was
    // reached.
    minutes_last = minutes;
    if (seconds >= 60) {
        Serial.print("ESP.getFreeHeap(): ");
        Serial.println(ESP.getFreeHeap());
        Serial.print("Nb fetch: ");
        Serial.println(nbFetch);
        timeLast = timeNow;
        minutes = minutes + 1;
        fetch = true;
        // checkRestart();
    }

    // the number of seconds that have passed since the last time 60 seconds was
    // reached.
    if (delaySinceActSec == 60) {
        timeLast2 = timeNow;
        delaySinceActMin = delaySinceActMin + 1;
    }

    if ((minutes % DELAY_FETCH == 0 && fetch) || (gogogofetch)) {
        if (!decoder->isRunning()) {
            checkUpdateSounds();
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

	if (letsgo) {
		digitalWrite(D2, HIGH);
	} else {
		digitalWrite(D2, LOW);
	}

    if (capteur->isTriggered(delaySinceActMin, delaySinceActSec, timeLast2,
                             timeNow, letsgo)) {
		infraredActivation = false;
        if (!letsgo) {
            Serial.println(F("Lancement du son apres delai before"));
            delay(delayBefSecSet * 1000);
            if (currentIndex == max_sound) {
                currentIndex = 0;
                randomizeAll();
            }
            String pathString = allSoundsStored[allIndexes[currentIndex]].path;
            char path[64];
            pathString.toCharArray(path, 64);
            Serial.print(F("Titre: "));
            Serial.println(allSoundsStored[allIndexes[currentIndex++]].title);
            Serial.print(F("Path: "));
            Serial.println(path);
            if (decoder->isRunning()) {
                decoder->stop();
            }
            source->close();
            source->open(path);
            decoder->begin(source, output);
        }
        letsgo = true;
        static int lastms = 0;
        if (decoder->isRunning()) {
            if (millis() - lastms > 1000) {
                lastms = millis();
                Serial.printf("Running for %d ms...\n", lastms);
                Serial.flush();
            }
            if (!decoder->loop()) decoder->stop();
        } else {
            Serial.println(F("MP3 done\n"));
            delay(1000);
            letsgo = false;
            timeLast2 = timeNow;
            delaySinceActSec = 0;
            delaySinceActMin = 0;
        }
    }
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

void randomizeAll() { 
    Serial.print(F("max_sound: "));
    Serial.println(max_sound);
    for (unsigned char i = 0; i < max_sound; i++) {  // fill array
        allIndexes[i] = i;
        Serial.printf("%d,", allIndexes[i]);
    }
    Serial.println(F("\n done with population \n"));
    Serial.println(F("here is the final array\n"));

    for (unsigned char i = 0; i < max_sound; i++) {  // shuffle array
        unsigned char temp = allIndexes[i];
        unsigned char randomIndex = rand() % max_sound;

        allIndexes[i] = allIndexes[randomIndex];
        allIndexes[randomIndex] = temp;
    }

    for (unsigned char i = 0; i < max_sound; i++) {  // print array
        Serial.printf("%d,", allIndexes[i]);
    }
}

void fetchAudiosOnline() {
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
    Serial.print(F("[HTTPS] begin...\n"));
    if (https.begin(
            *client,
            F("https://connect.midi-agency.com/module/tracks?id_module=") +
                idModule)) {
        // HTTP header has been send and Server response header has been handled
        int httpCode = https.GET();
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

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
        Serial.print(entry.name());
        if (strcmp(entry.name(), "System Volume Information") == 0) continue;
        if (entry.name()[0] == '.') continue;
        Serial.print("\t\t");
        Serial.println(entry.size(), DEC);

        allSoundsStored[index].path = entry.fullName();
        allSoundsStored[index].size = entry.size();
        allSoundsStored[index++].title = entry.name();

        entry.close();
    }
    max_sound = index - 1;
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