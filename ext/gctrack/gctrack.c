#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

typedef struct record_t record_t;

struct record_t {
  uint32_t cycles;
  uint64_t duration;
  record_t *parent;
};

static VALUE tracepoint = Qnil;

static record_t *last_record = NULL;
static uint64_t last_enter = 0;

static uint64_t 
nanotime()
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
    rb_sys_fail("clock_gettime");
  }
  return ts.tv_sec * (uint64_t) 1000000000 + ts.tv_nsec;
}

static inline void 
add_gc_cycle(uint64_t duration)
{
  record_t *record = last_record;
  while (record) {
    record->cycles = record->cycles + 1;
    record->duration = record->duration + duration;
    record = record->parent;
  }
}

static inline bool 
gctracker_enabled() 
{
  return !NIL_P(tracepoint) && rb_tracepoint_enabled_p(tracepoint);
}

static void 
gctracker_hook(VALUE tpval, void *data)
{
  if (!gctracker_enabled() && !last_record) {
    return;
  }
  rb_trace_arg_t *tparg = rb_tracearg_from_tracepoint(tpval);
  switch (rb_tracearg_event_flag(tparg)) {
    case RUBY_INTERNAL_EVENT_GC_ENTER: {
      if (gctracker_enabled()) {
        last_enter = nanotime();
      }
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

static bool
create_tracepoint() 
{
  rb_event_flag_t events;
  events = RUBY_INTERNAL_EVENT_GC_ENTER | RUBY_INTERNAL_EVENT_GC_EXIT;
  tracepoint = rb_tracepoint_new(0, events, gctracker_hook, (void *) NULL);
  if (NIL_P(tracepoint)) {
    return false;
  }
  rb_global_variable(&tracepoint);
  return true;
}

static VALUE
gctracker_start_record(int argc, VALUE *argv, VALUE klass)
{
  if(!gctracker_enabled()) {
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
  if (!last_record) {
    return Qnil;
  } 
  record_t *record = last_record;
  last_record = record->parent;
  
  VALUE stats = rb_ary_new2(2);
  rb_ary_store(stats, 0, ULONG2NUM(record->cycles));
  rb_ary_store(stats, 1, ULONG2NUM(record->duration));

  free(record);  

  return stats;
}

static VALUE
gctracker_enable(int argc, VALUE *argv, VALUE klass)
{
  if (gctracker_enabled()) {
    return Qtrue;
  }

  if (NIL_P(tracepoint)) {
    if(!create_tracepoint()) {
      return Qfalse;
    }
  }

  rb_tracepoint_enable(tracepoint);
  if (!gctracker_enabled()) {
    return Qfalse;
  }

  return Qtrue;
}

static VALUE
gctracker_disable(VALUE self)
{
  if (!gctracker_enabled()) {
    return Qfalse;
  }

  rb_tracepoint_disable(tracepoint);
  if (gctracker_enabled()) {
    rb_raise(rb_eRuntimeError, "GCTracker: Couldn't disable tracepoint!");
  } 
  
  return Qtrue;
}

void
Init_gctrack()
{
  VALUE mGC = rb_define_module("GC");
  VALUE cTracker = rb_define_module_under(mGC, "Tracker");

  rb_define_module_function(cTracker, "enable", gctracker_enable, 0);
  rb_define_module_function(cTracker, "disable", gctracker_disable, 0);

  rb_define_module_function(cTracker, "start_record", gctracker_start_record, 0);
  rb_define_module_function(cTracker, "end_record", gctracker_end_record, 0);
}
