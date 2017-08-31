#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

typedef struct {
  unsigned long cycles;
  unsigned long long duration;
  void *parent;
} record_t;

static VALUE cTracker;
static ID id_tracepoint;

static bool enabled = false;

static record_t *last_record = NULL;
static unsigned long long last_enter = 0;

static unsigned long long 
nanotime()
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
    rb_sys_fail("clock_gettime");
  }
  return ts.tv_sec * (unsigned long long) 1000000000 + ts.tv_nsec;
}

static inline void 
add_gc_cycle(unsigned long long duration)
{
  record_t *record = last_record;
  while (record) {
    record->cycles = record->cycles + 1;
    record->duration = record->duration + duration;
    record = (record_t *) record->parent;
  }
}

static void 
gctracker_hook(VALUE tpval, void *data)
{
  if (!enabled) {
    return;
  }
  rb_trace_arg_t *tparg = rb_tracearg_from_tracepoint(tpval);
  switch (rb_tracearg_event_flag(tparg)) {
    case RUBY_INTERNAL_EVENT_GC_ENTER: {
      last_enter = nanotime();
    }
      break;
    case RUBY_INTERNAL_EVENT_GC_EXIT: {
      if (last_enter) {
        add_gc_cycle(nanotime() - last_enter);
      }
      last_enter = 0;
    }
      break;
  }
}

static VALUE
gctracker_start_record(int argc, VALUE *argv, VALUE klass)
{
  if(!enabled) {
    return Qfalse;
  }

  record_t *record = (record_t *) calloc(1, sizeof(record_t));
  if (!record) {
    return Qfalse;
  }
  
  record->parent = last_record;
  last_record = record;
  return Qtrue;
}

static VALUE
gctracker_end_record(int argc, VALUE *argv, VALUE klass)
{
  if (last_record) {
    record_t *record = last_record;
    last_record = (record_t *) record->parent;
    
    VALUE stats = rb_ary_new2(2);
    rb_ary_store(stats, 0, ULONG2NUM(record->cycles));
    rb_ary_store(stats, 1, ULONG2NUM(record->duration));
  
    free(record);  

    return stats;
  }
  return Qnil;
}

static VALUE
gctracker_enable(int argc, VALUE *argv, VALUE klass)
{
  VALUE tracepoint = rb_ivar_get(cTracker, id_tracepoint);
  if (NIL_P(tracepoint)) {
    enabled = true;
    rb_event_flag_t events;
    events = RUBY_INTERNAL_EVENT_GC_ENTER | RUBY_INTERNAL_EVENT_GC_EXIT;
    tracepoint = rb_tracepoint_new(0, events, gctracker_hook, (void *) NULL);
    if (NIL_P(tracepoint)) { 
      enabled = false;
      return Qfalse;
    }
    rb_ivar_set(cTracker, id_tracepoint, tracepoint);
    rb_tracepoint_enable(tracepoint);
  } else if (!enabled) {
    enabled = true;
    rb_raise(rb_eTypeError, "GCTracker: CORRUPTED FSM!");
  }
  
  return Qtrue;
}

static VALUE
gctracker_disable(VALUE self)
{
  VALUE tracepoint = rb_ivar_get(cTracker, id_tracepoint);  
  if (NIL_P(tracepoint)) {
    if (enabled) {
      enabled = false;
      rb_raise(rb_eTypeError, "GCTracker: CORRUPTED FSM!");
    }
    return Qfalse;
  }

  rb_tracepoint_disable(tracepoint);
  rb_ivar_set(cTracker, id_tracepoint, Qnil);
  enabled = false;
  while (last_record) {
    record_t *record = last_record;
    last_record = (record_t *) record->parent;
    free(record);
  }
  last_record = NULL;
  
  return Qtrue;
}

void
Init_gctrack()
{
  VALUE mGC = rb_define_module("GC");
  cTracker = rb_define_module_under(mGC, "Tracker");

  id_tracepoint = rb_intern("__gc_tracepoint");  
  rb_ivar_set(cTracker, id_tracepoint, Qnil);

  rb_define_module_function(cTracker, "enable", gctracker_enable, 0);
  rb_define_module_function(cTracker, "disable", gctracker_disable, 0);

  rb_define_module_function(cTracker, "start_record", gctracker_start_record, 0);
  rb_define_module_function(cTracker, "end_record", gctracker_end_record, 0);
}
