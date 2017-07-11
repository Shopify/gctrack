#include <ruby.h>
#include <ruby/debug.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

typedef struct gc_event_struct gc_event_t;
struct gc_event_struct {
  gc_event_t *next;
  rb_event_flag_t ev;
  struct timespec ts;
};

typedef struct {
  VALUE block;
  gc_event_t *events;
} flush_gc_data_t;

static VALUE mTracker = Qnil;
static ID idCall;
static bool enabled = false;
static gc_event_t *gc_events = NULL;
static gc_event_t *gc_current_event = NULL;

static gc_event_t * alloc_event()
{
  gc_event_t *ev = (gc_event_t *) calloc(1, sizeof(gc_event_t));
  return ev;
}

static void
flush_gc_events(void *arg)
{
  flush_gc_data_t *flush_data = (flush_gc_data_t *) arg;

  rb_funcall(flush_data->block, idCall, 0);

  free(flush_data);
}

static void
gc_event_hook(rb_event_flag_t event, VALUE data, VALUE self, ID id, VALUE klass)
{
  flush_gc_data_t *flush_data = NULL;
  gc_event_t *ev = alloc_event();

  clock_gettime(CLOCK_MONOTONIC_RAW, &ev->ts);
  ev->ev = event;

  if (!gc_events) {
    gc_events = gc_current_event = ev;
    flush_data = (flush_gc_data_t *) calloc(1, sizeof(flush_gc_data_t));
    flush_data->block = data;
    flush_data->events = gc_events;
    rb_postponed_job_register(0, flush_gc_events, flush_data);
  } else {
    gc_current_event->next = ev;
    gc_current_event = ev;
  }
}

static VALUE
enable(int argc, VALUE *argv, VALUE obj)
{
  VALUE block = Qnil;
  rb_event_flag_t events = RUBY_INTERNAL_EVENT_GC_ENTER |
    RUBY_INTERNAL_EVENT_GC_EXIT;

  if (enabled) {
    return Qfalse;
  }

  rb_scan_args(argc, argv, "&", &block);
  if (block == Qnil) {
    rb_raise(rb_eArgError, "block required");
  }

  rb_add_event_hook(gc_event_hook, events, block);

  enabled = true;

  return Qtrue;
}

static VALUE
disable(VALUE self)
{
  if (!enabled) {
    return Qfalse;
  }

  int ret = rb_remove_event_hook(gc_event_hook);
  if (ret != 1) {
    rb_raise(rb_eRuntimeError, "error removing event hook: %d", ret);
  }

  enabled = false;

  return Qtrue;
}

void
Init_gctrack()
{
  VALUE mGC = rb_define_module("GC");
  mTracker = rb_define_module_under(mGC, "Tracker");

  rb_define_singleton_method(mTracker, "enable", enable, -1);
  rb_define_singleton_method(mTracker, "disable", disable, 0);

  idCall = rb_intern("call");
}
