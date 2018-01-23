#include <ruby.h>
#include <ruby/intern.h>
#include "actuator.h"
#include "ruby_helpers.h"

VALUE empty_array_value;

VALUE rb_proc_call_fast(VALUE block)
{
    return rb_proc_call(block, empty_array_value);
}

void init_ruby_helpers()
{
    empty_array_value = rb_ary_new();
    rb_gc_register_address(&empty_array_value);
}