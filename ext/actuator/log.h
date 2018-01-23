#ifndef ACTUATOR_LOG_H
#define ACTUATOR_LOG_H

#include "actuator.h"

class Log {
public:
    static void Setup();
    static void Debug(const char *format, ...);
    static void Info(const char *format, ...);
    static void Warn(const char *format, ...);
    static void Error(const char *format, ...);

    static FILE *debug_file;
    static FILE *log_file;
};

#endif