#include <iostream>
#include <map>
#include <queue>
#include <ruby.h>
#include "actuator.h"
#include "timer.h"
#include "log.h"
#include "ruby_helpers.h"

class Actuator
{
public:
    bool is_running = false;
    bool is_sleeping = false;
    bool is_waking = false;
    VALUE thread = 0;

    const double max_delta = 0.05;

    double sleep_ended_at = 0;
    double total_late = 0;

    std::queue<VALUE> next_tick_queue;

    Actuator();
    ~Actuator();

    void Start();
    void Stop();
    void Wake();
    timeval GetNextEventDelay(double now);
};

extern Actuator *actuator;