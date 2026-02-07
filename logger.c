#include "logger.h"
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/**
 * Thread-safe, cross-process logger.
 * - Uses a POSIX message queue so forked children can enqueue log lines quickly (non-blocking).
 * - A dedicated logger thread in the parent drains the queue and writes to game.log.
 * - A named semaphore guards the file in case another process ever writes directly.
 */

static const char *LOG_FILE_PATH = "game.log";
static const char *LOG_MQ_NAME   = "/monopoly_log_mq";
static const char *LOG_SEM_NAME  = "/monopoly_log_sem";
static const long  LOG_MSG_SIZE  = 1024;
static const long  LOG_MAX_MSGS  = 10;

static pthread_t log_thread;
static mqd_t mq_read = (mqd_t)-1;
static mqd_t mq_write = (mqd_t)-1;
static sem_t *log_sem = NULL;
static FILE *log_fp = NULL;
static int is_running = 0;
static pid_t owner_pid = 0;

static void format_timestamp(char *buf, size_t buflen) {
    struct timespec ts;
    struct tm tm_local;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm_local);
    snprintf(buf, buflen, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
             tm_local.tm_year + 1900,
             tm_local.tm_mon + 1,
             tm_local.tm_mday,
             tm_local.tm_hour,
             tm_local.tm_min,
             tm_local.tm_sec,
             ts.tv_nsec / 1000000);
}

static void write_log_line(const char *line) {
    if (log_sem != NULL) {
        sem_wait(log_sem);
    }

    if (log_fp != NULL) {
        fputs(line, log_fp);
        fflush(log_fp);
    }

    if (log_sem != NULL) {
        sem_post(log_sem);
    }
}

static void *logger_thread_main(void *arg) {
    (void)arg;
    char buf[LOG_MSG_SIZE];

    while (1) {
        ssize_t n = mq_receive(mq_read, buf, sizeof(buf), NULL);
        if (n >= 0) {
            buf[n] = '\0';
            if (strcmp(buf, "__LOGGER_EXIT__") == 0) {
                break;
            }
            write_log_line(buf);
            continue;
        }

        if (errno == EINTR) {
            continue;
        }

        // If queue was closed/unlinked, exit.
        break;
    }

    return NULL;
}

int logger_init(const char *path) {
    if (is_running) {
        return 0;
    }

    owner_pid = getpid();

    const char *file_path = (path != NULL) ? path : LOG_FILE_PATH;
    log_fp = fopen(file_path, "a");
    if (log_fp == NULL) {
        fprintf(stderr, "[LOGGER] Error: failed to open %s: %s\n", file_path, strerror(errno));
        return -1;
    }

    log_sem = sem_open(LOG_SEM_NAME, O_CREAT, 0666, 1);
    if (log_sem == SEM_FAILED) {
        fprintf(stderr, "[LOGGER] Warning: sem_open failed: %s\n", strerror(errno));
        log_sem = NULL;
    }

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = LOG_MAX_MSGS;
    attr.mq_msgsize = LOG_MSG_SIZE;
    attr.mq_curmsgs = 0;

    mq_unlink(LOG_MQ_NAME); // ensure clean slate
    mq_read = mq_open(LOG_MQ_NAME, O_CREAT | O_RDONLY, 0666, &attr);
    if (mq_read == (mqd_t)-1) {
        fprintf(stderr, "[LOGGER] Error: mq_open (read) failed: %s\n", strerror(errno));
        if (log_sem) { sem_close(log_sem); sem_unlink(LOG_SEM_NAME); }
        fclose(log_fp);
        log_fp = NULL;
        return -1;
    }

    mq_write = mq_open(LOG_MQ_NAME, O_WRONLY | O_NONBLOCK);
    if (mq_write == (mqd_t)-1) {
        fprintf(stderr, "[LOGGER] Error: mq_open (write) failed: %s\n", strerror(errno));
        mq_close(mq_read);
        mq_unlink(LOG_MQ_NAME);
        if (log_sem) { sem_close(log_sem); sem_unlink(LOG_SEM_NAME); }
        fclose(log_fp);
        log_fp = NULL;
        return -1;
    }

    is_running = 1;
    if (pthread_create(&log_thread, NULL, logger_thread_main, NULL) != 0) {
        fprintf(stderr, "[LOGGER] Error: failed to start logger thread\n");
        is_running = 0;
        mq_close(mq_read);
        mq_close(mq_write);
        mq_unlink(LOG_MQ_NAME);
        if (log_sem) { sem_close(log_sem); sem_unlink(LOG_SEM_NAME); }
        fclose(log_fp);
        log_fp = NULL;
        return -1;
    }

    logger_log("=== MONOPOLY GAME LOGGER STARTED ===");
    printf("[LOGGER] Logger thread started (TID: %lu)\n", (unsigned long)log_thread);

    return 0;
}

void logger_shutdown(void) {
    if (!is_running) {
        return;
    }

    // Signal thread to exit
    mq_send(mq_write, "__LOGGER_EXIT__", strlen("__LOGGER_EXIT__"), 0);

    pthread_join(log_thread, NULL);

    mq_close(mq_read);
    mq_close(mq_write);
    mq_unlink(LOG_MQ_NAME);

    if (log_sem != NULL) {
        sem_close(log_sem);
        sem_unlink(LOG_SEM_NAME);
        log_sem = NULL;
    }

    if (log_fp != NULL) {
        fclose(log_fp);
        log_fp = NULL;
    }

    is_running = 0;
}

void logger_log(const char *fmt, ...) {
    // Allow forked children to log: they inherit mq_write descriptor if init happened before fork.
    if (!is_running || mq_write == (mqd_t)-1) {
        return;
    }

    char timestamp[64];
    format_timestamp(timestamp, sizeof(timestamp));

    char message_buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message_buf, sizeof(message_buf), fmt, args);
    va_end(args);

    char line_buf[LOG_MSG_SIZE];
    snprintf(line_buf, sizeof(line_buf), "%s [%d] %s\n", timestamp, (int)getpid(), message_buf);

    // Non-blocking send; if full, drop to avoid stalling gameplay.
    if (mq_send(mq_write, line_buf, strlen(line_buf), 0) == -1) {
        // Optionally could count drops; for now just ignore to stay non-blocking.
    }
}
