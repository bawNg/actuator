#include <ruby.h>
#include "reactor.h"

enum class LogLevel
{
    None,
    Error,
    Warn,
    Info,
    Debug
};

FILE *Log::debug_file = 0;
FILE *Log::log_file = 0;

static VALUE LogClass;
static LogLevel Level = LogLevel::Info;

static void Print(const char *tag, const char *format, va_list args)
{
    if (!Log::log_file) return;
    fprintf(Log::log_file, "%010.3f %s ", clock_time() * 1000, tag);
    vfprintf(Log::log_file, format, args);
    fprintf(Log::log_file, "\n");
    fflush(Log::log_file);
}

static VALUE Log_Debug(VALUE self, VALUE message)
{
    Log::Debug("%s", StringValueCStr(message));
    return Qnil;
}

static VALUE Log_Info(VALUE self, VALUE message)
{
    Log::Info("%s", StringValueCStr(message));
    return Qnil;
}

static VALUE Log_Warn(VALUE self, VALUE message)
{
    Log::Warn("%s", StringValueCStr(message));
    return Qnil;
}

static VALUE Log_Error(VALUE self, VALUE message)
{
    Log::Error("%s", StringValueCStr(message));
    return Qnil;
}

static VALUE Log_SetFilePath(VALUE self, VALUE path)
{
    if (NIL_P(path)) {
        Log::log_file = 0;
    } else if (SYMBOL_P(path) && SYM2ID(path) == rb_intern("stdout")) {
        Log::log_file = stdout;
    } else if (RB_TYPE_P(path, T_STRING) && CLASS_OF(path) == rb_cString) {
        Log::log_file = fopen(RSTRING_PTR(path), "w");
    } else {
        rb_raise(rb_eRuntimeError, "path must be a string, :stdout or nil");
    }
    return Qnil;
}

static VALUE Log_SetLevel(VALUE self, VALUE level)
{
    if (SYMBOL_P(level)) {
        if (SYM2ID(level) == rb_intern("debug")) {
            Level = LogLevel::Debug;
            return Qnil;
        }
        if (SYM2ID(level) == rb_intern("info")) {
            Level = LogLevel::Info;
            return Qnil;
        }
        if (SYM2ID(level) == rb_intern("warn")) {
            Level = LogLevel::Warn;
            return Qnil;
        }
        if (SYM2ID(level) == rb_intern("error")) {
            Level = LogLevel::Error;
            return Qnil;
        }
    }
    rb_raise(rb_eRuntimeError, "level must be :debug, :info, :warn or :error");
    return Qnil;
}

void Log::Setup()
{
    log_file = stdout;

    LogClass = rb_define_module("Log");

    rb_define_singleton_method(LogClass, "debug", RUBY_METHOD_FUNC(Log_Debug), 1);
    rb_define_singleton_method(LogClass, "puts", RUBY_METHOD_FUNC(Log_Info), 1);
    rb_define_singleton_method(LogClass, "warn", RUBY_METHOD_FUNC(Log_Warn), 1);
    rb_define_singleton_method(LogClass, "error", RUBY_METHOD_FUNC(Log_Error), 1);
    rb_define_singleton_method(LogClass, "file_path=", RUBY_METHOD_FUNC(Log_SetFilePath), 1);
    rb_define_singleton_method(LogClass, "level=", RUBY_METHOD_FUNC(Log_SetLevel), 1);
}

void Log::Debug(const char *format, ...)
{
    if (Level < LogLevel::Debug) return;
    va_list args;
    va_start(args, format);
    Print("DEBUG", format, args);
    va_end(args);
}

void Log::Info(const char *format, ...)
{
    if (Level < LogLevel::Info) return;
    va_list args;
    va_start(args, format);
    Print("INFO", format, args);
    va_end(args);
}

void Log::Warn(const char *format, ...)
{
    if (Level < LogLevel::Warn) return;
    va_list args;
    va_start(args, format);
    Print("WARN", format, args);
    va_end(args);
}

void Log::Error(const char *format, ...)
{
    if (Level < LogLevel::Error) return;
    va_list args;
    va_start(args, format);
    Print("ERROR", format, args);
    va_end(args);
}