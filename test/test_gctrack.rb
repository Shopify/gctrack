require 'test/unit'
require 'gctrack/gctrack'

class TestGctrack < Test::Unit::TestCase
  def test_enable
    GC::Tracker.enable { }
  ensure
    GC::Tracker.disable
  end

  def test_returns_false_when_not_enabled
    assert_equal false, GC::Tracker.disable
  end

  def test_enabled_generates_events
    GC::Tracker.enable { puts "gc event" }
    GC.start
  ensure
    GC::Tracker.disable
  end
end

