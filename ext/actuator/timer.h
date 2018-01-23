#include <iostream>
#include <map>
#include <ruby.h>

#include "actuator.h"
#include "clock.h"

class Timer {
public:
    int id;
    double delay;
    double interval;
    double at;
    VALUE fiber;
    VALUE instance = 0;
    VALUE callback_block;
    std::multimap<double, Timer*>::iterator iterator;
    bool is_scheduled;
    bool is_destroyed;
    char* inspected;

    Timer();
    Timer(double initial_delay);
    ~Timer();
    void Destroy();
    void SetDelay(double initial_delay);
    void Schedule();
    void Remove();
    void SetCallback(VALUE callback);
    void SetFiber(VALUE current_fiber);
    void SetInitialDelay(VALUE delay);
    void ExpireImmediately();
    void Fire();

    static void Setup();
    static Timer* Get(VALUE instance);
    static void Clear();
    static void Update(double now);
    static double GetNextEventTime();
private:
    void InsertIntoSchedule();
    bool RemoveFromSchedule();
    void StartedBeingScheduled();
    void StoppedBeingScheduled();
};