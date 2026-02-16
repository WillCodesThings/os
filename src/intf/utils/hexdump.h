#pragma once

#include <stdint.h>
#include <utils/log.h>

// Dump `len` bytes at `addr` to serial in classic hex+ASCII format.
// 16 bytes per line. No heap dependency.
void hexdump(const void *addr, uint32_t len);

// Hexdump that respects log level.
void hexdump_log(log_level_t level, const char *tag, const void *addr, uint32_t len);

// Convenience macro — compiles out in release builds
#ifdef DEBUG
#define LOG_HEXDUMP(tag, addr, len) hexdump_log(LOG_DEBUG, tag, addr, len)
#else
#define LOG_HEXDUMP(tag, addr, len) ((void)0)
#endif
