#pragma once

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SD.h>
#include <WiFi.h>

// Log levels
enum LOG_LEVEL {
    LOG_SUCCESS,
    LOG_WARNING,
    LOG_ERROR,
    LOG_INFO  // Default log level
};

class Logger {
public:
    Logger(wifi_mode_t wifiMode, NTPClient *timeClient);
    void printLog(const char* function, LOG_LEVEL level, bool logToSD, const char* message, ...);

private:
    int sdCSPin;
    String currentLogFile;
    wifi_mode_t wifiMode;  // Store the WiFi mode
    NTPClient *timeClient;  // Pointer to the global timeClient

    const char* logDirectory = "/logs";
    const uint64_t maxLogSize = 1048576;       // 1MB log file size

    static const int maxFiles = 10;  // Maximum number of log files
    static const int eepromAddress = 0;  // EEPROM address for storing the last file ID
    
    uint8_t previousID = 0;  // Current file ID
    uint8_t currentID = 0;  // Current file ID

    uint8_t getLastFileID();
    void saveCurrentFileID(uint8_t id);
    void renamePreviousLogFile();

    String getCurrentDate();
    void createLogFile();
    void deleteOldLogFile();
    void checkAndRotateLogFile();
};
