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
    assert_equal 0, GC::Tracker.cycles
    assert GC::Tracker.enable
    GC.start
    assert GC::Tracker.cycles > 0
  ensure
    GC::Tracker.disable
    assert GC::Tracker.cycles == 0
  end
end

