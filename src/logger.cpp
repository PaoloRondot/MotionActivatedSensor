#include "logger.hpp"

Logger::Logger(wifi_mode_t wifiMode, NTPClient *timeClient)
    : wifiMode(wifiMode), timeClient(timeClient) {
    currentLogFile = "";
}

String Logger::getCurrentDate() {
    if (!timeClient->update()) {
        timeClient->forceUpdate();  // Force an update from the NTP server
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

// Function to generate a file name based on an incrementing ID if WiFi is off
int Logger::getNextLogFileID() {
    int nextID = 0;
    File dir = SD.open(logDirectory);
    
    if (!dir || !dir.isDirectory()) {
        SD.mkdir(logDirectory);  // Create log directory if it doesn't exist
        return nextID;
    }

    // Search for the highest numbered log file
    while (File file = dir.openNextFile()) {
        if (!file.isDirectory()) {
            String fileName = file.name();
            int startIdx = fileName.lastIndexOf('/') + 1;
            int endIdx = fileName.indexOf('.', startIdx);
            
            if (endIdx > startIdx) {
                String idStr = fileName.substring(startIdx, endIdx);
                int id = idStr.toInt();
                if (id >= nextID) {
                    nextID = id + 1;  // Increment for the next log file
                }
            }
        }
        file.close();
    }
    dir.close();

    return nextID;
}

// Function to calculate the total size of the .logs folder
uint64_t Logger::getLogFolderSize() {
    uint64_t totalSize = 0;
    File dir = SD.open(logDirectory);
    
    if (!dir || !dir.isDirectory()) {
        Serial.println("[ERROR] .logs directory not found.");
        return 0;
    }

    while (File file = dir.openNextFile()) {
        if (!file.isDirectory()) {
            totalSize += file.size();  // Add the size of each file to the total
        }
        file.close();
    }
    dir.close();
    return totalSize;
}

// Delete a log file in the log directory that is not the current log file
void Logger::deleteOldLogFile() {
    // Open the log directory
    File dir = SD.open(logDirectory); // Assuming logDirectory is defined as "/.logs"
    if (!dir || !dir.isDirectory()) {
        Serial.println("[ERROR] Log directory not found.");
        return;
    }

    bool deleted = false; // Flag to track if a file is deleted
    while (File file = dir.openNextFile()) {
        if (!file.isDirectory() && String(file.name()) != currentLogFile) {
            Serial.print("[INFO] Deleting log file: ");
            Serial.println(file.name());
            SD.remove(file.name()); // Delete the file
            deleted = true; // Mark that we deleted a file
            break; // Exit after deleting one file
        }
        file.close(); // Close the file
    }

    if (!deleted) {
        Serial.println("[INFO] No old log files found to delete.");
    }

    dir.close(); // Close the directory
}

void Logger::createLogFile() {
    if (wifiMode == WIFI_OFF) {
        // Generate file name based on ID if WiFi is off
        int logID = getNextLogFileID();
        currentLogFile = String(logDirectory) + "/" + String(logID) + ".log";
    } else {
        // Create a new log file based on the date using NTP if WiFi is on
        String dateStr = getCurrentDate();
        currentLogFile = String(logDirectory) + "/" + dateStr + ".log";
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
            deleteOldLogFile();  // Delete the oldest log file
            currentLogFile = "";  // Reset log file so that a new one is created
            createLogFile();
        } else {
            logFile.close();
        }
    } else {
        Serial.println("[ERROR] Could not open log file.");
    }

    // Check if the total log folder size exceeds the 10MB limit
    uint64_t folderSize = getLogFolderSize();
    while (folderSize > maxLogFolderSize) {
        Serial.println("[INFO] Log folder size exceeds 10MB, deleting oldest log file.");
        deleteOldLogFile();  // Delete the oldest file
        folderSize = getLogFolderSize();  // Recalculate folder size after deletion
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