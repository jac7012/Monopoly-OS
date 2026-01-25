#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

/**
 * Logger Module Header
 *
 * Thread-safe, non-blocking (for gameplay) logging via a dedicated logger thread.
 * Call logger_init() once at startup, then use logger_log() for events.
 * Call logger_shutdown() during cleanup to flush and stop the logger thread.
 */

int logger_init(const char *path);
void logger_shutdown(void);
void logger_log(const char *fmt, ...);

#endif // LOGGER_H
