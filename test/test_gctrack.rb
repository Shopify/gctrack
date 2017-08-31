require 'test/unit'
require 'gctrack/gctrack'

class TestGctrack < Test::Unit::TestCase
  def test_enable
    assert GC::Tracker.enable
  ensure
    GC::Tracker.disable
  end

  def test_disable
    assert !GC::Tracker.disable
    assert GC::Tracker.enable
    assert GC::Tracker.enable
    assert GC::Tracker.disable
    assert !GC::Tracker.disable
  end

  def test_returns_false_when_not_enabled
    assert_equal false, GC::Tracker.disable
  end

  def test_enabled_generates_events
    assert GC::Tracker.enable
    assert GC::Tracker.start_record
    GC.start
    cycles, duration = GC::Tracker.end_record
    assert cycles > 0
    assert duration > 0
  ensure
    GC::Tracker.disable
  end
end

