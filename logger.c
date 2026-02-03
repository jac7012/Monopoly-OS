#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define LOG_FILE_PATH "game.log"

static FILE *log_fp = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;


int logger_init(const char *path) {
    const char *file_path = (path != NULL) ? path : LOG_FILE_PATH;
    log_fp = fopen(file_path, "a");
    
    if (!log_fp) {
        fprintf(stderr, "[LOGGER] Error: failed to open %s\n", file_path);
        return -1;
    }
    
    // Write startup marker
    logger_log("=== MONOPOLY GAME LOGGER STARTED ===");
    return 0;
}

void logger_shutdown(void) {
    if (log_fp) {
        logger_log("=== MONOPOLY GAME LOGGER SHUTDOWN ===");
        fclose(log_fp);
        log_fp = NULL;
    }
}

void logger_log(const char *fmt, ...) {
    if (!log_fp) {
        return;
    }
    
    // Lock to ensure atomic write - prevents interleaving
    pthread_mutex_lock(&log_mutex);
    
    // Get current time for timestamp
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Write timestamp to file
    fprintf(log_fp, "[%s] ", timestamp);
    
    // Write formatted message to file
    va_list args;
    va_start(args, fmt);
    vfprintf(log_fp, fmt, args);
    va_end(args);
    
    // Add newline and flush immediately for ordered output
    fprintf(log_fp, "\n");
    fflush(log_fp);
    
    // Also print to console for visibility
    printf("[LOG] ");
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    
    pthread_mutex_unlock(&log_mutex);
}
