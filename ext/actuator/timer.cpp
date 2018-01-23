#include "reactor.h"

const unsigned int MaxOutstandingTimers = 1000000;

static int late_warning_us;

static int total_count = 0;
static int current_timer_count = 0;
static int current_object_count = 0;
static int current_gc_registered_count = 0;
static int fired_current_second_count = 0;
static int fired_last_second_count = 0;
static int current_second_frame_count = 0;
static int last_second_frame_count = 0;
static int current_second_empty_frames = 0;
static int last_second_empty_frames = 0;
static int current_second_earliest_fire = INT_MAX;
static int last_second_earliest_fire = INT_MAX;
static int current_second_latest_fire = 0;
static int last_second_latest_fire = 0;
static int current_second_started_at = 0;

//TODO: Consider replacing multimap with a high precision timer wheel implementation like the one I built in C#
static std::multimap<double, Timer*> all;
static std::deque<Timer*> expired_queue;
static std::deque<Timer*> interval_queue;

static VALUE TimerClass;
static VALUE proc_call_args[2];

static void Timer_free(Timer *timer)
{
    current_object_count--;
    timer->Destroy();
    delete timer;
}

static void Timer_mark(Timer *timer)
{
    if (timer->callback_block)
        rb_gc_mark(timer->callback_block);
    else if (timer->fiber)
        rb_gc_mark(timer->fiber);
}

static VALUE Timer_alloc(VALUE klass)
{
    Timer *timer = new Timer();
    current_object_count++;
    return timer->instance = Data_Wrap_Struct(klass, Timer_mark, Timer_free, timer);
}

static VALUE Timer_initialize(VALUE self)
{
    Timer *timer = Timer::Get(self);
    if (rb_block_given_p()) timer->SetCallback(rb_block_proc());
    return self;
}

static VALUE Timer_destroy(VALUE self)
{
    Timer *timer = Timer::Get(self);
    if (timer)
        timer->Destroy();
    else
        rb_raise(rb_eRuntimeError, "Timer instance has no allocated timer struct");
    return Qnil;
}

static VALUE Timer_expires_at(VALUE self)
{
    return DBL2NUM(Timer::Get(self)->at);
}

static VALUE Timer_is_destroyed(VALUE self)
{
    return (Timer::Get(self))->is_destroyed ? Qtrue : Qfalse;
}

static VALUE Timer_fire_bang(VALUE self)
{
    Timer *timer = Timer::Get(self);
    if (timer->is_destroyed || !timer->is_scheduled) return Qnil;
    timer->ExpireImmediately();
    return Qtrue;
}

static VALUE Timer_in(VALUE self, VALUE delay_value)
{
    rb_need_block();
    Log::Debug("Timer.in");
    Timer *timer = new Timer(NUM2DBL(delay_value));
    timer->SetCallback(rb_block_proc());
    timer->Schedule();
    current_object_count++;
    // We skip calling initialize on the timer to reduce overhead
    return timer->instance = Data_Wrap_Struct(TimerClass, Timer_mark, Timer_free, timer);
}

static VALUE Timer_every(VALUE self, VALUE delay_value)
{
    rb_need_block();
    Log::Debug("Timer.every");
    double delay = NUM2DBL(delay_value);
    Timer *timer = new Timer(delay);
    timer->interval = delay;
    timer->SetCallback(rb_block_proc());
    timer->Schedule();
    current_object_count++;
    // We skip calling initialize on the timer to reduce overhead
    return timer->instance = Data_Wrap_Struct(TimerClass, Timer_mark, Timer_free, timer);
}

static VALUE Timer_late_warning_us(VALUE self)
{
    return INT2NUM(late_warning_us);
}

static VALUE Timer_late_warning_us_set(VALUE self, VALUE value)
{
    return INT2NUM(late_warning_us = NUM2INT(value));
}

static VALUE Timer_stats(VALUE self)
{
    return rb_sprintf("Frames: %d, Empty: %d, Fires: %d, Early: %d, Late: %d, Current: %d, Objects: %d, Scheduled: %d, GC: %d, Total: %d", last_second_frame_count, last_second_empty_frames, fired_last_second_count, last_second_earliest_fire < INT_MAX ? last_second_earliest_fire : -1, last_second_latest_fire, current_timer_count, current_object_count, all.size(), current_gc_registered_count, total_count);
}

void Timer::Setup()
{
    proc_call_args[0] = Qnil;
    proc_call_args[1] = empty_array_value;

    TimerClass = rb_define_class("Timer", rb_cObject);
    rb_define_singleton_method(TimerClass, "in", RUBY_METHOD_FUNC(Timer_in), 1);
    rb_define_singleton_method(TimerClass, "every", RUBY_METHOD_FUNC(Timer_every), 1);
    rb_define_singleton_method(TimerClass, "stats", RUBY_METHOD_FUNC(Timer_stats), 0);
    rb_define_singleton_method(TimerClass, "late_warning_us", RUBY_METHOD_FUNC(Timer_late_warning_us), 0);
    rb_define_singleton_method(TimerClass, "late_warning_us=", RUBY_METHOD_FUNC(Timer_late_warning_us_set), 1);
    rb_define_alloc_func(TimerClass, Timer_alloc);
    rb_define_method(TimerClass, "initialize", RUBY_METHOD_FUNC(Timer_initialize), 0);
    rb_define_method(TimerClass, "expires_at", RUBY_METHOD_FUNC(Timer_expires_at), 0);
    rb_define_method(TimerClass, "destroy", RUBY_METHOD_FUNC(Timer_destroy), 0);
    rb_define_method(TimerClass, "destroyed?", RUBY_METHOD_FUNC(Timer_is_destroyed), 0);
    rb_define_method(TimerClass, "fire!", RUBY_METHOD_FUNC(Timer_fire_bang), 0);

    late_warning_us = 0;
    current_second_started_at = clock_time();
}

Timer* Timer::Get(VALUE instance)
{
    Timer *timer;
    Data_Get_Struct(instance, Timer, timer);
    return timer;
}

void Timer::Update(double now)
{
    std::multimap<double, Timer*>::iterator it = all.begin();
    for (; it != all.end(); ++it) {
		if (it->first > now) break;
		//if (now - it->first > 0.000005) {
		    //long late = (now - it->first) * 1000000;
		    //puts("%.10f - Timer expired: %.10f, Late: %d us", now, it->first, late);
		//}
		Timer *timer = it->second;
        if (!timer->is_scheduled) Log::Error("Expired timer %d has is_scheduled set to false!", timer->id);
		expired_queue.push_back(timer);
	}

    int active_count = all.size();
    int expired_count = expired_queue.size();

    current_second_frame_count++;
    if (expired_count < 1) current_second_empty_frames++;
    fired_current_second_count += expired_count;
    if (now >= current_second_started_at + 5.0)
    {
        current_second_started_at = now;
        last_second_frame_count = current_second_frame_count;
        current_second_frame_count = 0;
        last_second_empty_frames = current_second_empty_frames;
        current_second_empty_frames = 0;
        fired_last_second_count = fired_current_second_count;
        fired_current_second_count = 0;
        last_second_earliest_fire = current_second_earliest_fire;
        current_second_earliest_fire = INT_MAX;
        last_second_latest_fire = current_second_latest_fire;
        current_second_latest_fire = 0;
    }

    if (expired_count < 1) return;

    Log::Debug("Update - %d / %d timers expiring", expired_count, active_count);

    if (expired_count < active_count)
        all.erase(all.begin(), it);
    else
        all.clear();

    std::deque<Timer*>::iterator deq = expired_queue.begin();
    while (deq != expired_queue.end()) {
        Timer *timer = (Timer*)*deq++;
        timer->is_scheduled = false;
        Log::Debug("Update - Expired");
        if (timer->interval) {
            //Log::Debug("Update - Firing: %s", RSTRING_PTR(rb_inspect(timer->callback_block)));
            timer->Fire();
            if (timer->is_destroyed)
            {
                Log::Debug("Update - Interval destroyed from it's own callback");
                timer->StoppedBeingScheduled();
            }
            else
            {
                Log::Debug("Update - Adding to interval queue");
                interval_queue.push_back(timer);
            }
        } else {
            timer->is_destroyed = true;
            timer->StoppedBeingScheduled();
            timer->Fire();
        }
        if (!actuator->is_running) break;
    }
    expired_queue.clear();

    if (!actuator->is_running) return;

    now = clock_time();
    deq = interval_queue.begin();
    while (deq != interval_queue.end()) {
        Timer *timer = (Timer*)*deq++;
        if (timer->is_destroyed) {
            Log::Debug("Update - Interval destroyed from another timers callback");
            timer->StoppedBeingScheduled();
            continue;
        }
        Log::Debug("Update - Rescheduling interval");
        timer->at = now + timer->interval;
        timer->InsertIntoSchedule();
    }
    interval_queue.clear();

    Log::Debug("Update - Done");
}

double Timer::GetNextEventTime()
{
    return all.empty() ? 0 : all.begin()->first;
}

Timer::Timer()
{
    id = ++total_count;
    delay = 0;
    at = 0;
    interval = 0;
    callback_block = 0;
    fiber = 0;
    is_scheduled = false;
    is_destroyed = false;
    instance = 0;
    inspected = 0;
    current_timer_count++;
}

Timer::Timer(double initial_delay) : Timer()
{
    delay = initial_delay;
    at = clock_time() + initial_delay;
}

Timer::~Timer()
{
    if (callback_block) {
        //TODO: Profile overhead of unregister when loaded (walks the entire global object list)
        rb_gc_unregister_address(&callback_block);
        callback_block = 0;
        if (inspected) {
            free(inspected);
            inspected = 0;
        }
    }
    if (is_scheduled) Log::Error("Timer freed while still scheduled");
    if (!is_destroyed) Log::Error("Timer freed before being destroyed");
    current_timer_count--;
}

void Timer::SetDelay(double initial_delay)
{
    Log::Debug("SetDelay");
    delay = initial_delay;
    at = clock_time() + initial_delay;
}

void Timer::Destroy()
{
    if (is_destroyed) return;
    Log::Debug("Destroy");
    is_destroyed = true;
    Remove();
}

void Timer::Schedule()
{
    if (!at) {
        Log::Warn("Timer::Schedule() called before delay was set");
        return;
    }
    if (all.size() > MaxOutstandingTimers) {
        Log::Warn("Error: There are %d / %d active timers!", all.size(), MaxOutstandingTimers);
        return;
    }
    InsertIntoSchedule();
    StartedBeingScheduled();
}

void Timer::Remove()
{
    Log::Debug("Remove");
    if (RemoveFromSchedule())
        StoppedBeingScheduled();
}

void Timer::InsertIntoSchedule()
{
    if (is_scheduled) return;
    Log::Debug("InsertIntoSchedule");
    is_scheduled = true;
    iterator = all.insert(std::make_pair(at, this));
}

bool Timer::RemoveFromSchedule()
{
    if (!is_scheduled) return false;
    Log::Debug("RemoveFromSchedule");
    is_scheduled = false;
    all.erase(iterator);
    return true;
}

void Timer::StartedBeingScheduled()
{
    Log::Debug("StartedBeingScheduled");
    rb_gc_register_address(&instance);
    current_gc_registered_count++;
}

void Timer::StoppedBeingScheduled()
{
    Log::Debug("StoppedBeingScheduled");
    rb_gc_unregister_address(&instance);
    current_gc_registered_count--;
    Log::Debug("GC pointer count: %d", current_gc_registered_count);
}

void Timer::SetCallback(VALUE block)
{
    Log::Debug("SetCallback");
    if (callback_block) {
        rb_gc_unregister_address(&callback_block);
        free(inspected);
        inspected = 0;
    }
    callback_block = block;
    if (block) {
        rb_gc_register_address(&callback_block);
        //VALUE obj = rb_inspect(block);
        //inspected = (char*)malloc(RSTRING_LEN(obj)+1);
        //strcpy(inspected, RSTRING_PTR(obj));
    }
}

void Timer::SetFiber(VALUE current_fiber)
{
    fiber = current_fiber;
}

void Timer::ExpireImmediately()
{
    if (is_destroyed || !is_scheduled) return;
    Log::Debug("ExpireImmediately");
    if (interval) {
        RemoveFromSchedule();
        Fire();
        if (is_destroyed) {
            StoppedBeingScheduled();
        } else {
            at = clock_time() + interval;
            InsertIntoSchedule();
        }
    } else {
        is_destroyed = true;
        Remove();
        Fire();
    }
}

static VALUE fire_rescue(VALUE _, VALUE errinfo)
{
    Log::Debug("Uncaught exception, stopping reactor");
    actuator->Stop();
    Log::Debug("Raising uncaught exception");
    rb_exc_raise(errinfo);
    Log::Debug("Uncaught exception has been raised");
    return Qnil;
}

void Timer::Fire()
{
    double before_call = clock_time();

    double before_resume;
    if (callback_block)
    {
        double late_us = (double)((before_call - at) * 1000000);
        if ((int)late_us < current_second_earliest_fire) current_second_earliest_fire = (int)late_us;
        if ((int)late_us > current_second_latest_fire) current_second_latest_fire = (int)late_us;
        if (late_warning_us && late_us > late_warning_us) {
            Log::Warn("Firing %.2f us late - %d active timers, %d fired last second", late_us, all.size(), fired_last_second_count);
        }
        int rescue_state;
        proc_call_args[0] = callback_block;
        rb_rescue(RUBY_METHOD_FUNC(rb_proc_call_fast), callback_block, RUBY_METHOD_FUNC(fire_rescue), Qnil);
    }
    else if (fiber)
    {
        Log::Warn("[Fire] Resuming fiber %.2f us late", (double)((before_call - at) * 1000000));
        if (!rb_fiber_alive_p(fiber))
        {
            Log::Error("[Fire] Unable to resume fiber (not alive)");
            return;
        }
        before_resume = clock_time();
        VALUE ret = DBL2NUM(before_resume);
        rb_fiber_resume(fiber, 1, &ret);
        delete this; //TODO: Shouldn't this happen before calling rb_fiber_resume
    }
    // Assume at least 0.3us overhead for ruby to call into C
    //double after_call = clock_time();
    //puts("[FireTimer] call took %.2f us, resume: %.2f us, resume_total: %.2f us, late: %.2f us", (double)((after_call - before_call) * 1000000), (double)((sleep_ended_at - before_resume) * 1000000) - 0.3, (double)((after_call - before_resume) * 1000000) - 0.3, (double)((sleep_ended_at - at) * 1000000) - 0.3);
}

void Timer::Clear()
{
    //TODO: Actually clean up and free all timers and next_tick callbacks
    all.clear();
}