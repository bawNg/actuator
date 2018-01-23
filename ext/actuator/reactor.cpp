#include "reactor.h"

Actuator *actuator = 0;

Actuator::Actuator()
{
}

Actuator::~Actuator()
{
}

void Actuator::Start()
{
    if (is_running) {
        Log::Warn("[Actuator] Start called while already running");
        return;
    }

    thread = rb_thread_current();
    is_running = true;

    if (rb_block_given_p()) rb_yield(Qundef);

    long total_ticks = 0;

    double now = clock_time();

    while (is_running)
    {
        total_ticks++;

        Timer::Update(now);

        int remaining_dequeues = next_tick_queue.size();
        while (remaining_dequeues--)
        {
            VALUE proc = next_tick_queue.front();
            next_tick_queue.pop();
            rb_proc_call_fast(proc);
        }

        if (!is_running) break;

        now = clock_time();

        timeval delay_duration;
        double next_timer_at = Timer::GetNextEventTime();
        if (next_timer_at) {
            double delay = next_timer_at - now;
            if (delay > 0) {
                if (delay > max_delta) {
                    // Never sleep for longer than max_delta
                    delay_duration.tv_sec = 0;
                    delay_duration.tv_usec = max_delta * 1000000;
                } else {
                    // Extra delay avoids most wake ups that would happen before any timers expire due to bad sleep precision
                    delay_duration.tv_sec = (long)delay;
                    delay_duration.tv_usec = (((long)(delay * 1000000)) % 1000000) + 1;
                }
            } else {
                // Sleep for as few microseconds as possible so that we give other ruby threads
                // GVL time while also waking up to process expired timers as soon as possible
                delay_duration.tv_sec = delay_duration.tv_usec = 0;
            }
        } else {
            delay_duration.tv_sec = 0;
            delay_duration.tv_usec = max_delta * 1000000;
        }

        is_sleeping = true;

        // rb_thread_wait_for has far better precision on Windows builds than using undocumented kernel system calls
        rb_thread_wait_for(delay_duration);

        is_waking = false;
        is_sleeping = false;

        /*double after = clock_time();
        if (next_timer_at && after < next_timer_at)
        {
            //double delay = (next_timer_at - after) * 1000000;
            //Log::Warn("Woke up %.2f us early - Slept for %.2f / %d us", delay, (after - now) * 1000000.0, delay_duration.tv_usec);
        } else {
            double late = (after - now) * 1000000 - delay_duration.tv_usec;
            if (late > 25) Log::Warn("Woke up %.2f us late - Slept for %.2f / %d us - Timer delay: %.2f", late, (after - now) * 1000000.0, delay_duration.tv_usec, (next_timer_at - now) * 1000000.0);
        }*/

        /*double sleep_delta = after - now;
        double late = (sleep_delta * 1000000) - (delay_duration.tv_sec * 1000000 + delay_duration.tv_usec);
        if (late > 2) Log::Warn("%.10f - Woke up after %.10f - %.2f us late", after, sleep_delta, late);*/

        now = clock_time();
    }

    thread = 0;
}

void Actuator::Stop()
{
    if (!is_running) {
        Log::Warn("[Actuator] Actuator.stop called while not running");
        return;
    }
    is_running = false;
    Timer::Clear();
    if (is_sleeping) Wake();
}

void Actuator::Wake()
{
    if (!is_sleeping || is_waking) return;
    is_waking = true;
    rb_thread_wakeup(thread);
}

static VALUE Actuator_now(VALUE klass)
{
    return DBL2NUM(clock_time());
}

static VALUE Actuator_is_running(VALUE klass)
{
    return actuator->is_running ? Qtrue : Qfalse;
}

static VALUE Actuator_start(VALUE klass)
{
    if (actuator->is_running) {
        if (rb_block_given_p()) rb_yield(Qundef);
    } else {
        actuator->Start();
    }
    return Qnil;
}

static VALUE Actuator_stop(VALUE klass)
{
    actuator->Stop();
    return Qnil;
}

static VALUE Actuator_wake(VALUE klass)
{
    actuator->Wake();
    return Qnil;
}

static VALUE Actuator_next_tick(VALUE self)
{
    //TODO: Prevent proc from being GC'd while scheduled
    rb_need_block();
    actuator->next_tick_queue.push(rb_block_proc());
    //rb_gc_register_address(&next_tick_queue.back());
}

static VALUE deferred_fiber(VALUE curr, VALUE block)
{
    rb_proc_call_fast(block);
}

static VALUE Actuator_defer(VALUE self)
{
    //TODO: Pool fibers and prevent them from being GC'd while scheduled
    rb_need_block();
    VALUE fiber = rb_fiber_new(RUBY_METHOD_FUNC(deferred_fiber), rb_block_proc());
    if (rb_thread_current() == actuator->thread)
    {
        rb_fiber_resume(fiber, 0, 0);
    }
    else
    {
        Timer *timer = new Timer();
        timer->SetFiber(fiber);
        timer->SetDelay(0);
        timer->Schedule();
    }
    return fiber;
}

static VALUE Actuator_sleep(VALUE self, VALUE delay_value)
{
    //TODO: Prevent fibers from being GC'd while scheduled
    Timer *timer = new Timer();
    timer->SetFiber(rb_fiber_current());
    timer->SetDelay(NUM2DBL(delay_value));
    timer->Schedule();
    VALUE nil = Qnil;
    return rb_fiber_yield(1, &nil);
}

extern "C"
void Init_actuator()
{
    init_ruby_helpers();
    clock_init();

    actuator = new Actuator();

    Log::Setup();
    Timer::Setup();

    VALUE ActuatorClass = rb_define_module("Actuator");
    rb_define_singleton_method(ActuatorClass, "now", RUBY_METHOD_FUNC(Actuator_now), 0);
    rb_define_singleton_method(ActuatorClass, "running?", RUBY_METHOD_FUNC(Actuator_is_running), 0);
    rb_define_singleton_method(ActuatorClass, "start", RUBY_METHOD_FUNC(Actuator_start), 0);
    rb_define_singleton_method(ActuatorClass, "stop", RUBY_METHOD_FUNC(Actuator_stop), 0);
    rb_define_singleton_method(ActuatorClass, "wake", RUBY_METHOD_FUNC(Actuator_wake), 0);
    //rb_define_singleton_method(ActuatorClass, "next_tick", RUBY_METHOD_FUNC(Actuator_next_tick), 0);
    //rb_define_singleton_method(ActuatorClass, "defer", RUBY_METHOD_FUNC(Actuator_defer), 0);
    //rb_define_singleton_method(FiberClass, "sleep", RUBY_METHOD_FUNC(Actuator_sleep), 1);
}