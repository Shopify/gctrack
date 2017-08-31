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

static stat_record *last_record = NULL;
static unsigned long last_enter;

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
  if (!enabled || !last_record) {
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
start_record(int argc, VALUE *argv, VALUE klass)
{
  stat_record *record = calloc(1, sizeof(start_record));
  if (!record) {
    return Qfalse;
  }    
  record->parent = last_record;
  last_record = record;
  return Qtrue;
}

static VALUE
end_record(int argc, VALUE *argv, VALUE klass)
{
  if (last_record) {
    stat_record *record = last_record;
  
    VALUE stats = rb_ary_new2(2);
    rb_ary_store(stats, 0, ULONG2NUM(record->cycles));
    rb_ary_store(stats, 1, ULONG2NUM(record->duration));
  
    last_record = record->parent;
    free(record);  

    return stats;
  }
  return Qnil;
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

  while (last_record) {
    stat_record *record = last_record;
    last_record = record->parent;
    free(record);
  }

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

  rb_define_singleton_method(cTracker, "start_record", start_record, 0);
  rb_define_singleton_method(cTracker, "end_record", end_record, 0);
}
