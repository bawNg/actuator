#ifndef ACTUATOR_DEBUG_H
#define ACTUATOR_DEBUG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG 0

#ifdef DEBUG
#define puts(...) do { \
    fprintf(stdout, __VA_ARGS__); \
    fprintf(stdout, "\n"); \
    fflush(stdout); \
} while (0)
#else
#define puts(...)
#endif

#ifdef DEBUG
#define debug(...) do { \
    if (!debug_file) debug_file = fopen("debug.txt", "w"); \
    fprintf(debug_file, __VA_ARGS__); \
    fflush(debug_file); \
} while (0)
#define DEBUG_LOG(...) do { \
    if (!debug_file) debug_file = fopen("debug.txt", "w"); \
    fprintf(debug_file, __VA_ARGS__); \
    fflush(debug_file); \
} while (0)
#else
#define debug(...)
#define DEBUG_LOG(...)
#endif

#define INFO(...) do { \
    fprintf(log_file, "%010.3f INFO ", clock_time() * 1000); \
    fprintf(log_file, __VA_ARGS__); \
    fprintf(log_file, "\n"); \
    fflush(log_file); \
} while (0)

#define WARN(...) do { \
    fprintf(log_file, "%010.3f WARN ", clock_time() * 1000); \
    fprintf(log_file, __VA_ARGS__); \
    fprintf(log_file, "\n"); \
    fflush(log_file); \
} while (0)

#ifdef __cplusplus
}
#endif

#endif
