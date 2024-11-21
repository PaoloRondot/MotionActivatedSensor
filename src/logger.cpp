#include "logger.hpp"
#include <EEPROM.h>

Logger::Logger(wifi_mode_t wifiMode, NTPClient *timeClient)
    : wifiMode(wifiMode), timeClient(timeClient) {
    currentLogFile = "";
    createLogFile();
}

String Logger::getCurrentDate() {
    if (wifiMode != WIFI_OFF) {
        if (!timeClient->update()) {
            timeClient->forceUpdate();  // Force an update from the NTP server
        }
    }

    unsigned long epochTime = timeClient->getEpochTime();
    struct tm *ptm = gmtime((time_t *)&epochTime);  // Use gmtime to get UTC time

    // Create a formatted string for date and time
    char dateStr[30]; // Increased size to accommodate the full timestamp
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d_%02d-%02d-%02d", 
        ptm->tm_year + 1900, 
        ptm->tm_mon + 1, 
        ptm->tm_mday,
        ptm->tm_hour,
        ptm->tm_min,
        ptm->tm_sec);

    return String(dateStr);
}

// Delete a log file in the log directory that is not the current log file
void Logger::deleteOldLogFile() {
    // Open the log directory
    File dir = SD.open(logDirectory); // Assuming logDirectory is defined as "/.logs"
    if (!dir || !dir.isDirectory()) {
        Serial.println("[ERROR] Log directory not found.");
        return;
    }
    char id_str;
    itoa(currentID, &id_str, 10);
    while (File file = dir.openNextFile()) {
        if (!file.isDirectory()) {
            if (file.name()[0] == id_str) {
                Serial.printf("[INFO] Deleting old log file: %s with ID: %d\n", file.path(), currentID);
                SD.remove(file.path());
                break;
            }
        }
        file.close(); // Close the file
    }
}

// Retrieve the last used file ID from EEPROM
uint8_t Logger::getLastFileID() {
    EEPROM.begin(1);
    uint8_t id = EEPROM.read(eepromAddress);
    Serial.printf("[INFO] Last file ID: %d\n", id);
    EEPROM.end();
    return id % maxFiles;
}

// Save the last used file ID to EEPROM
void Logger::saveCurrentFileID(uint8_t id) {
    EEPROM.begin(1);
    EEPROM.write(eepromAddress, id);
    EEPROM.commit();
    EEPROM.end();
}

// Rename the current log file to remove the `_` marker
void Logger::renamePreviousLogFile() {
    // Open the log directory
    File dir = SD.open(logDirectory); // Assuming logDirectory is defined as "/.logs"
    if (!dir || !dir.isDirectory()) {
        Serial.println("[ERROR] Log directory not found.");
        return;
    }

    while (File file = dir.openNextFile()) {
        if (!file.isDirectory()) {
            String path = file.path();
            String path_save = file.path();
            if (path.endsWith("_.log")) {
                path.remove(path.length() - 5, 1); // Remove the `_`
                Serial.printf("[INFO] Renaming file: %s to %s\n", path_save.c_str(), path.c_str());
                SD.rename(path_save, path);
                break;
            }
        }
        file.close(); // Close the file
    }
}

void Logger::createLogFile() {
    previousID = getLastFileID();
    currentID = (previousID + 1) % maxFiles;
    saveCurrentFileID(currentID);
    deleteOldLogFile();
    renamePreviousLogFile();
    if (wifiMode == WIFI_OFF) {
        // Generate file name based on ID if WiFi is off
        currentLogFile = String(logDirectory) + "/" + String(currentID) + ".log";
    } else {
        // Create a new log file based on the date using NTP if WiFi is on
        String dateStr = getCurrentDate();
        currentLogFile = String(logDirectory) + "/" + String(currentID) + "_" + dateStr + "_" + ".log";
    }
    File logFile = SD.open(currentLogFile, FILE_WRITE);
    logFile.close();
}

// Function to check and ensure log file size does not exceed the limits
void Logger::checkAndRotateLogFile() {
    if (currentLogFile.isEmpty()) {
        createLogFile();
    }
    File logFile = SD.open(currentLogFile, FILE_READ);
    if (logFile) {
        if (logFile.size() > maxLogSize) {
            Serial.println("[INFO] Log file exceeded 1MB, rotating log.");
            logFile.close();
            createLogFile();
        } else {
            logFile.close();
        }
    } else {
        Serial.println("[ERROR] Could not open log file.");
    }
}

void Logger::printLog(const char* function, LOG_LEVEL level, bool logToSD, const char* message, ...) {
    static uint8_t logCounter = 0;  // Counter to track the number of logs
    // Define the log message prefix based on log level
    String logMessage;
    switch(level) {
        case LOG_SUCCESS:
            logMessage += "\x1b[32m" "[SUCCESS] ";
            break;
        case LOG_WARNING:
            logMessage += "\x1b[33m" "[WARNING] ";
            break;
        case LOG_ERROR:
            logMessage += "\x1b[31m" "[ERROR] ";
            break;
        default:
            logMessage += "[INFO] ";
            break;
    }

    logMessage += function;
    logMessage += " : ";

    // Format the message with variable arguments
    va_list args;
    va_start(args, message);
    char buffer[256];
    vsnprintf(buffer, 256, message, args);
    va_end(args);

    logMessage += buffer;
    logMessage += "\x1b[0m";  // Reset the color

    // Print to Serial
    Serial.println(logMessage);

    // If logToSD is true, write to the SD card
    if (logToSD) {
        // Ensure the log directory exists
        if (!SD.exists(logDirectory)) {
            if (!SD.mkdir(logDirectory)) {
                Serial.println("[ERROR] Failed to create log directory on SD card.");
                return;
            }
        }

        if (logCounter == 0) {
            checkAndRotateLogFile();  // Check if we need to rotate the log file
        }

        // Open the log file and write the log message
        File logFile = SD.open(currentLogFile, FILE_APPEND);
        if (logFile) {
            logFile.println(logMessage);  // Write log message to SD card
            logFile.close();  // Close the file
        } else {
            Serial.println("[ERROR] Could not open log file on SD card.");
        }
    }
    logCounter++;  // Increment the log counter
}