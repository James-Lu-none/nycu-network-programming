#ifndef LOGGING_H
#define LOGGING_H

#include "colors.h"
#include <time.h>
#include <stdio.h>

#define LOG_INFO(fmt, ...) \
    do { \
        time_t now = time(NULL); \
        struct tm *t = localtime(&now); \
        char time_buf[64]; \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t); \
        printf(GRAY "[%s] " RESET BOLD CYAN "[INFO] " RESET fmt "\n", time_buf, ##__VA_ARGS__); \
        fflush(stdout); \
    } while (0)

#define LOG_DEBUG(fmt, ...) \
    do { \
        time_t now = time(NULL); \
        struct tm *t = localtime(&now); \
        char time_buf[64]; \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t); \
        printf(GRAY "[%s] " RESET MAGENTA "[DEBUG] " RESET fmt "\n", time_buf, ##__VA_ARGS__); \
        fflush(stdout); \
    } while (0)

#define LOG_ERROR(fmt, ...) \
    do { \
        time_t now = time(NULL); \
        struct tm *t = localtime(&now); \
        char time_buf[64]; \
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", t); \
        fprintf(stderr, GRAY "[%s] " RESET BOLD RED "[ERROR] " RESET fmt "\n", time_buf, ##__VA_ARGS__); \
        fflush(stderr); \
    } while (0)

#endif // LOGGING_H