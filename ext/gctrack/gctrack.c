#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

static VALUE s_total_allocated_objects;

typedef struct record_t record_t;

struct record_t {
  uint32_t cycles;
  uint64_t duration;
  size_t allocations;
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

/*
 * call-seq:
 *    GC::Tracker.start_record  -> true or false
 *
 * Starts recording GC cycles and their duration. Returns +true+ if the 
 * recording was started, +false+ otherwise. If the record could be started
 * an GC::Tracker#end_record invocation will always return data for the record, 
 * otherwise that invocation would return +nil+.
 * One case where +false+ would be returned is if the Tracker isn't enabled.
 * #start_record can be nested. These records will have a parent-child relationship,
 * where the parent will contain all GC data of the child.
 *
 *    GC::Tracker.enabled?      # true
 *    GC::Tracker.start_record  # true
 *    GC::Tracker.start_record  # true
 *    GC::Tracker.end_record    # [2, 12654, 254] from the second #start_record
 *    GC::Tracker.disable       # true
 *    GC::Tracker.start_record  # false
 *    GC::Tracker.end_record    # [3, 15782, 537] from the first #start_record, contains the data from the first record
 *    GC::Tracker.end_record    # nil from the last #start_record, that returned false
 */
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
  last_record->allocations = rb_gc_stat(s_total_allocated_objects);
  return Qtrue;
}

/*
 * call-seq:
 *    GC::Tracker.end_record  -> Array or nil
 *
 * Ends the recording of the last started record and return the 
 * the collected information. 
 * The array contains three numbers: The amount of GC cycles observed,
 * the cumulative duration in nanoseconds of these cycles and the amount of objects allocated. 
 * If multiple recordings were started they will be be popped from the stack of records and their 
 * values will be returned. 
 * GC data gathered is in all records of the stack. 
 *
 *    GC::Tracker.start_record  # true
 *    GC::Tracker.start_record  # true
 *    GC::Tracker.end_record    # [2, 12654, 34534] amount of GC cycles and duration as well as objects allocated since the second #start_record
 *    GC::Tracker.end_record    # [3, 15782, 35678] GC data & allocations since the first #start_record, including the data above
 *    GC::Tracker.disable       # true
 *    GC::Tracker.start_record  # false
 *    GC::Tracker.end_record    # nil
 */
static VALUE
gctracker_end_record(int argc, VALUE *argv, VALUE klass)
{
  if (!last_record) {
    return Qnil;
  } 
  record_t *record = last_record;
  last_record = record->parent;
  
  VALUE stats = rb_ary_new2(3);
  rb_ary_store(stats, 0, ULONG2NUM(record->cycles));
  rb_ary_store(stats, 1, ULONG2NUM(record->duration));
  rb_ary_store(stats, 2, SIZET2NUM(rb_gc_stat(s_total_allocated_objects) - record->allocations));

  free(record);  

  return stats;
}

/*
 * call-seq:
 *    GC::Tracker.enabled?  -> true or false
 *
 * Returns +true+ if the Tracker is enabled, +false+ otherwise.
 *
 *    GC::Tracker.enabled?  # false
 *    GC::Tracker.enable    # true
 *    GC::Tracker.enabled?  # true
 *    GC::Tracker.enable    # true
 */
static VALUE 
gctracker_enabled_p(int argc, VALUE *argv, VALUE klass)
{
  return gctracker_enabled() ? Qtrue : Qfalse;
}

/*
 * call-seq:
 *    GC::Tracker.enable   -> true or false
 *
 * Globally enables the tracker. Returns +true+ if the Tracker is 
 * enabled after this call (whether it enabled it or not), +false+ otherwise.
 *
 *    GC::Tracker.disable   # true
 *    GC::Tracker.enabled?  # false
 *    GC::Tracker.enable    # true
 *    GC::Tracker.enabled?  # true
 */
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

/*
 * call-seq:
 *    GC::Tracker.disable   -> true or false
 *
 * Globally disables the tracker. Returns +true+ if the Tracker is 
 * disabled after this call (whether it disabled it or not), +false+ otherwise.
 *
 *    GC::Tracker.enabled?  # true
 *    GC::Tracker.disable   # true
 *    GC::Tracker.enabled?  # false
 *    GC::Tracker.disable   # true
 */
static VALUE
gctracker_disable(VALUE self)
{
  if (!gctracker_enabled()) {
    return Qtrue;
  }

  rb_tracepoint_disable(tracepoint);
  if (gctracker_enabled()) {
    return Qfalse;
  } 
  
  return Qtrue;
}

void
Init_gctrack()
{
  s_total_allocated_objects = ID2SYM(rb_intern("total_allocated_objects"));

  VALUE mGC = rb_define_module("GC");
  VALUE cTracker = rb_define_module_under(mGC, "Tracker");

  rb_define_module_function(cTracker, "enable", gctracker_enable, 0);
  rb_define_module_function(cTracker, "enabled?", gctracker_enabled_p, 0);
  rb_define_module_function(cTracker, "disable", gctracker_disable, 0);

  rb_define_module_function(cTracker, "start_record", gctracker_start_record, 0);
  rb_define_module_function(cTracker, "end_record", gctracker_end_record, 0);
}
