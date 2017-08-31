#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

static VALUE mGC;
static ID id_tracepoint;

static bool enabled = false;
static unsigned long cycles = 0;

static VALUE
gc_cycles(int argc, VALUE *argv, VALUE klass)
{
  return ULONG2NUM(cycles);
}

static void gc_hook(VALUE tpval, void *data)
{
  if (!enabled) {
      return;
  }
  rb_trace_arg_t *tparg = rb_tracearg_from_tracepoint(tpval);
  switch (rb_tracearg_event_flag(tparg)) {
      case RUBY_INTERNAL_EVENT_GC_ENTER:
          break;
      case RUBY_INTERNAL_EVENT_GC_EXIT:
          cycles++;
          break;
  }
}

static VALUE
enable(int argc, VALUE *argv, VALUE klass)
{
  if (NIL_P(rb_ivar_get(mGC, id_tracepoint))) {
    rb_event_flag_t events;
    events = RUBY_INTERNAL_EVENT_GC_START | RUBY_INTERNAL_EVENT_GC_END_MARK | RUBY_INTERNAL_EVENT_GC_END_SWEEP;
    events |= RUBY_INTERNAL_EVENT_GC_ENTER | RUBY_INTERNAL_EVENT_GC_EXIT;
    VALUE tracepoint = rb_tracepoint_new(0, events, gc_hook, (void *)0);
    if (NIL_P(tracepoint)) { 
      return Qfalse;
    }
    rb_tracepoint_enable(tracepoint);
    rb_ivar_set(mGC, id_tracepoint, tracepoint);
  }
  
  enabled = true;
  return Qtrue;
}

static VALUE
disable(VALUE self)
{
  VALUE tracepoint = rb_ivar_get(mGC, id_tracepoint);  
  if (NIL_P(tracepoint)) {
    return Qfalse;
  }

  cycles = 0; // todo kill this!

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
  rb_define_singleton_method(cTracker, "cycles", gc_cycles, 0);
  rb_define_singleton_method(cTracker, "disable", disable, 0);
}
