#include <stdint.h>
#include "clock.h"
#include "debug.h"

#if !defined(__APPLE__) && !defined(__MACH__) && !defined(_WIN32)
#define HAVE_POSIX_TIMER
#include <time.h>
#ifdef CLOCK_MONOTONIC
#define CLOCKID CLOCK_MONOTONIC
#else
#define CLOCKID CLOCK_REALTIME
#endif
#endif

#if defined(__APPLE__) || defined(__MACH__)
#define HAVE_MACH_TIMER
#include <mach/mach_time.h>
#endif

#if defined(_WIN32) || defined(_MSC_VER)
#define HAVE_WIN32_TIMER
#include <windows.h>
#ifdef _MSC_VER
#include <Strsafe.h>
#endif
#endif

#if !defined(HAVE_POSIX_TIMER) && !defined(HAVE_MACH_TIMER) && !defined(HAVE_WIN32_TIMER)
#error Only Windows, Linux and OSX are supported
#endif

static uint64_t last_count;
static uint64_t frequency;

void clock_init()
{
#ifdef HAVE_POSIX_TIMER
    struct timespec freq, time;
    clock_getres(CLOCKID, &freq);
    clock_gettime(CLOCKID, &time);
    frequency = 1000000000LL * freq.tv_nsec;
    last_count = time.tv_sec * 1000000000LL + time.tv_nsec;
#endif
#ifdef HAVE_MACH_TIMER
    mach_timebase_info_data_t frequency_nsec;
    mach_timebase_info(&frequency_nsec);
    frequency = 1000000000LL * frequency_nsec.numer / frequency_nsec.denom;
    last_count = mach_absolute_time();
#endif
#ifdef HAVE_WIN32_TIMER
    {
        LARGE_INTEGER tmp;
        QueryPerformanceFrequency(&tmp);
        frequency = tmp.QuadPart;
        QueryPerformanceCounter(&tmp);
        last_count = tmp.QuadPart;
    }
#endif
    //puts("Timer resolution: %g ns", 1e9 / (double)frequency);
}

double clock_time()
{
    uint64_t now;
    double delta;
#ifdef HAVE_POSIX_TIMER
    struct timespec time;
    clock_gettime(CLOCKID, &time);
    now = time.tv_sec * 1000000000LL + time.tv_nsec;
    delta = (now - last_count) / (double)frequency;
#endif
#ifdef HAVE_MACH_TIMER
    now = mach_absolute_time();
    delta = (now - last_count) / (double)frequency;
#endif
#ifdef HAVE_WIN32_TIMER
    {
        LARGE_INTEGER tmp;
        QueryPerformanceCounter(&tmp);
        now = tmp.QuadPart;
        delta = (now - last_count) / (double)frequency;
    }
#endif
    return delta;
}