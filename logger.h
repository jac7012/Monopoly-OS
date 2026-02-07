#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

/**
 * Logger Module Header
 *
 * Thread-safe, non-blocking logging via a dedicated logger thread.
 * Uses POSIX message queue for cross-process logging (works across fork()).
 * Call logger_init() once at startup (starts thread automatically).
 * Use logger_log() for events from any process.
 * Call logger_shutdown() during cleanup to flush and stop the logger thread.
 */

int logger_init(const char *path);
void logger_shutdown(void);
void logger_log(const char *fmt, ...);

#endif // LOGGER_H
