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
    called = false

    GC::Tracker.enable do
      called = true
    end
    GC.start

    # TODO: There is some *weird* VM issue happening here, where the update to
    # `called` is not immediately visible until its been accessed. By reading
    # `called` here, it seems to somehow force the local to be updated.
    nil if called

    assert called
  ensure
    GC::Tracker.disable
  end
end

