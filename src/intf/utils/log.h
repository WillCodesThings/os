#pragma once

#include <stdint.h>

typedef enum {
    LOG_TRACE = 0,
    LOG_DEBUG = 1,
    LOG_INFO = 2,
    LOG_WARN = 3,
    LOG_ERROR = 4,
} log_level_t;

// Initialize the logging subsystem (calls serial_init internally)
void log_init(void);

// Set the minimum log level that will be output
void log_set_level(log_level_t level);

// Core logging function — use the macros below instead
void log_write(log_level_t level, const char *tag, const char *fmt, ...);

// TRACE and DEBUG are compiled out in release builds
#ifdef DEBUG
#define LOG_TRACE(tag, fmt, ...) log_write(LOG_TRACE, tag, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(tag, fmt, ...) log_write(LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_TRACE(tag, fmt, ...) ((void)0)
#define LOG_DEBUG(tag, fmt, ...) ((void)0)
#endif

#define LOG_INFO(tag, fmt, ...) log_write(LOG_INFO, tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...) log_write(LOG_WARN, tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) log_write(LOG_ERROR, tag, fmt, ##__VA_ARGS__)
