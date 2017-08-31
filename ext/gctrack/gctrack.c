#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

typedef struct {
  unsigned long cycles;
  unsigned long duration;
  void *parent;
} stat_record;

static VALUE mGC;
static ID id_tracepoint;

static bool enabled = false;
static stat_record *last_record;

static unsigned long last_enter;

// TODO DELETE THESE
static VALUE
gc_duration(int argc, VALUE *argv, VALUE klass)
{
  if (last_record) {
    return ULONG2NUM(last_record->duration);
  }
  return ULONG2NUM(0);  
}

static VALUE
gc_cycles(int argc, VALUE *argv, VALUE klass)
{
  if (last_record) {
    return ULONG2NUM(last_record->cycles);
  }
  return ULONG2NUM(0);  
}
// END TODO

static unsigned long nanotime()
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
    rb_sys_fail("clock_gettime");
  }
  return ts.tv_sec * 1e9 + ts.tv_nsec;
}

static void gc_hook(VALUE tpval, void *data)
{
  if (!enabled) {
      return;
  }
  rb_trace_arg_t *tparg = rb_tracearg_from_tracepoint(tpval);
  switch (rb_tracearg_event_flag(tparg)) {
      case RUBY_INTERNAL_EVENT_GC_ENTER:
        last_enter = nanotime();
        break;
      case RUBY_INTERNAL_EVENT_GC_EXIT:
        last_record->cycles++;
        last_record->duration += nanotime() - last_enter;
        break;
  }
}

static VALUE
enable(int argc, VALUE *argv, VALUE klass)
{
  VALUE tracepoint = rb_ivar_get(mGC, id_tracepoint);
  if (NIL_P(tracepoint)) {
    rb_event_flag_t events;
    events = RUBY_INTERNAL_EVENT_GC_START | RUBY_INTERNAL_EVENT_GC_END_MARK | RUBY_INTERNAL_EVENT_GC_END_SWEEP;
    events |= RUBY_INTERNAL_EVENT_GC_ENTER | RUBY_INTERNAL_EVENT_GC_EXIT;
    VALUE tracepoint = rb_tracepoint_new(0, events, gc_hook, (void *)0);
    if (NIL_P(tracepoint)) { 
      return Qfalse;
    }
    rb_tracepoint_enable(tracepoint);
    rb_ivar_set(mGC, id_tracepoint, tracepoint);
    enabled = true;
  } else {
    if (!enabled) {
      enabled = true;
      rb_raise(rb_eTypeError, "GC::Tracker: CORRUPTED FSM!");
    }
  }

  last_record = calloc(1, sizeof(stat_record));
  
  return Qtrue;
}

static VALUE
disable(VALUE self)
{
  VALUE tracepoint = rb_ivar_get(mGC, id_tracepoint);  
  if (NIL_P(tracepoint)) {
    if (enabled) {
      enabled = false;
      rb_raise(rb_eTypeError, "GC::Tracker: CORRUPTED FSM!");
    }
    return Qfalse;
  }

  free(last_record);
  last_record = NULL;

  rb_tracepoint_disable(tracepoint);
  rb_ivar_set(mGC, id_tracepoint, Qnil);
  enabled = false;
  
  return Qtrue;
}

void
Init_gctrack()
{
  mGC = rb_define_module("GC");
  VALUE cTracker = rb_define_class_under(mGC, "Tracker", rb_cData);

  id_tracepoint = rb_intern("__gc_tracepoint");  
  rb_ivar_set(mGC, id_tracepoint, Qnil);

  rb_define_singleton_method(cTracker, "enable", enable, 0);
  rb_define_singleton_method(cTracker, "disable", disable, 0);

  // TODO DELETE THESE
  rb_define_singleton_method(cTracker, "cycles", gc_cycles, 0);
  rb_define_singleton_method(cTracker, "duration", gc_duration, 0);
}
