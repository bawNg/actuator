#ifndef ACTUATOR_RUBY_HELPERS_H
#define ACTUATOR_RUBY_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

extern VALUE empty_array_value;

VALUE rb_proc_call_fast(VALUE block);
void init_ruby_helpers();

#ifdef __cplusplus
}
#endif

#endif
