#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

typedef struct gctrack_event_struct gctrack_event_t;
struct gctrack_event_struct {
  gctrack_event_t *next;
  rb_event_flag_t ev;
  struct timespec ts;
};

typedef struct {
  VALUE proc;
  VALUE ctx;
  gctrack_event_t *events;
} gctrack_hook_data_t;

static VALUE cTracker = Qnil;
static ID idCall;
static bool enabled = false;

static void
hook_data_mark(void *ptr)
{
  gctrack_hook_data_t *hook_data = (gctrack_hook_data_t *) ptr;
  rb_gc_mark(hook_data->proc);
  if (!NIL_P(hook_data->ctx)) {
    rb_gc_mark(hook_data->ctx);
  }
}

static void
hook_data_free(void *ptr)
{
  gctrack_hook_data_t *hook_data = (gctrack_hook_data_t *) ptr;
  gctrack_event_t *p = hook_data->events;

  while (p != NULL) {
    gctrack_event_t *cur = p;
    p = p->next;
    free(cur);
  }

  free(hook_data);
}

static size_t
hook_data_memsize(const void *ptr)
{
  gctrack_hook_data_t *data = (gctrack_hook_data_t *) ptr;
  size_t sz = sizeof(gctrack_hook_data_t);

  for (gctrack_event_t *p = data->events; p != NULL; p = p->next) {
    sz += sizeof(gctrack_event_t);
  }
  return sz;
}

static const rb_data_type_t gctrack_hook_data_type = {
  "gctrack_hook_data",
  { hook_data_mark, hook_data_free, hook_data_memsize },
  0, 0, RUBY_TYPED_FREE_IMMEDIATELY
};

static gctrack_event_t * alloc_event(gctrack_hook_data_t *data, bool *first)
{
  gctrack_event_t *ev = (gctrack_event_t *) calloc(1, sizeof(gctrack_event_t));
  *first = false;
  if (!data->events) {
    data->events = ev;
    *first = true;
  } else {
    gctrack_event_t *cur = data->events;
    while (cur) {
      if (cur->next == NULL) {
        cur->next = ev;
        break;
      } else {
        cur = cur->next;
      }
    }
  }
  return ev;
}

static void
flush_gc_events(void *arg)
{
  gctrack_hook_data_t *hook_data = arg;

  // TODO: convert gctrack_events to ruby array for the callback
  rb_proc_call(hook_data->proc, rb_ary_new());
}

static void
gc_event_hook(rb_event_flag_t event, VALUE data, VALUE self, ID id, VALUE klass)
{
  gctrack_hook_data_t *hook_data;
  gctrack_event_t *ev;
  bool first = false;

  TypedData_Get_Struct(data, gctrack_hook_data_t, &gctrack_hook_data_type, hook_data);
  ev = alloc_event(hook_data, &first);

  clock_gettime(CLOCK_MONOTONIC_RAW, &ev->ts);
  ev->ev = event;

  if (first) {
    rb_postponed_job_register(0, flush_gc_events, hook_data);
  }
}

static VALUE
enable(int argc, VALUE *argv, VALUE klass)
{
  gctrack_hook_data_t *data;
  VALUE obj;
  VALUE opts;
  VALUE ctx = Qnil;

  rb_event_flag_t events = RUBY_INTERNAL_EVENT_GC_ENTER |
    RUBY_INTERNAL_EVENT_GC_EXIT;

  if (enabled) {
    return Qfalse;
  }

  if (!rb_block_given_p()) {
    rb_raise(rb_eArgError, "called without a block");
  }

  rb_scan_args(argc, argv, ":", &opts);
  if (!NIL_P(opts)) {
    ID keys[2];
    VALUE values[2];

    keys[0] = rb_intern("context");
    keys[1] = rb_intern("events");
    rb_get_kwargs(opts, keys, 0, 2, values);
    if (values[0] != Qundef) {
      ctx = values[0];
    }
  }

  obj = TypedData_Make_Struct(klass, gctrack_hook_data_t, &gctrack_hook_data_type, data);
  data->proc = rb_block_proc();
  data->ctx = ctx;
  data->events = NULL;

  rb_add_event_hook(gc_event_hook, events, obj);

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
  cTracker = rb_define_class_under(mGC, "Tracker", rb_cData);

  rb_define_singleton_method(cTracker, "enable", enable, -1);
  rb_define_singleton_method(cTracker, "disable", disable, 0);

  idCall = rb_intern("call");
}
