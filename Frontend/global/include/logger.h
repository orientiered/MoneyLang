/// @file Logger
#ifndef LOGGER_H
#define LOGGER_H

#include "error_debug.h"

/*------------------LOGGER----------------------------------------------------*/
/*------------------WITH DYNAMIC LOG LEVEL TO DEBUG NECESSARY CODE------------*/
/*------------------orientiered MIPT 2024-------------------------------------*/

const size_t LOGGER_MAX_FILENAME_SIZE = 128;
const size_t LOGGER_CONVERSION_BUFFER_SIZE = 4096;

#define DEFAULT_LOGFILE_NAME "log"
#define LOGGER_LOCALE "ru_RU.UTF-8"
#define LOGS_DIR "logs/"

enum LogLevel {
    L_ZERO,     ///< Essential information
    L_DEBUG,    ///< Debug information
    L_EXTRA     ///< Debug++
};

enum LogMode {
    L_TXT_MODE,
    L_HTML_MODE
};

/// @brief Open log file
enum status logOpen(const char *fileName, enum LogMode mode);

/// @brief Disables buffering
//! Warning: makes write crazy slow
enum status logDisableBuffering();

/// @brief Flush all changes to file
enum status logFlush();

/// @brief Close log file
enum status logClose();

/// @brief Set log level
void setLogLevel(enum LogLevel level);

/// @brief Get log level
enum LogLevel getLogLevel();

/// @brief Print in log file with time signature
enum status logPrintWithTime(enum LogLevel level, bool copyToStderr, const char* fmt, ...);

/// @brief Print in log file
enum status logPrint(enum LogLevel level, bool copyToStderr, const char* fmt, ...);

/// @brief Print with color in html mode
enum status logPrintColor(enum LogLevel level, const char *color, const char *background, const char *fmt, ...);

/// @brief Print in log file with place in code
#define LOG_PRINT(level, ...)                                                                       \
    do {                                                                                            \
        logPrint(level, 0, "[DEBUG] %s:%d : %s \n", __FILE__, __LINE__, __PRETTY_FUNCTION__);       \
        logPrint(level, __VA_ARGS__);                                                               \
    } while(0)

#if defined(DISABLE_LOGGING)
    #define LOGGER_ON_DBG(...) ;
#else
    #define LOGGER_ON_DBG(...) __VA_ARGS__
#endif

#endif

