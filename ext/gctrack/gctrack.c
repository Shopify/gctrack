#include <ruby.h>
#include <ruby/debug.h>
#include <ruby/intern.h>
#include <stdbool.h>
#include <errno.h>
#include <time.h>

static bool enabled = false;
static unsigned long cycles = 0;

static VALUE
gc_cycles(int argc, VALUE *argv, VALUE klass)
{
  return ULONG2NUM(cycles);
}

static VALUE
enable(int argc, VALUE *argv, VALUE klass)
{
  enabled = true;

  return Qtrue;
}

static VALUE
disable(VALUE self)
{
  if (!enabled) {
    return Qfalse;
  }

  enabled = false;

  return Qtrue;
}

void
Init_gctrack()
{
  VALUE mGC = rb_define_module("GC");
  VALUE cTracker = rb_define_class_under(mGC, "Tracker", rb_cData);

  rb_define_singleton_method(cTracker, "enable", enable, 0);
  rb_define_singleton_method(cTracker, "cycles", gc_cycles, 0);
  rb_define_singleton_method(cTracker, "disable", disable, 0);
}
