require 'test/unit'
require 'gctrack'

class TestGctrack < Test::Unit::TestCase
  def test_enable
    assert GC::Tracker.enable
  ensure
    GC::Tracker.disable
  end

  def test_disable
    assert GC::Tracker.disable
    assert GC::Tracker.enable
    assert GC::Tracker.disable
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

  def test_recurses
    assert GC::Tracker.enable
    assert GC::Tracker.start_record
    a = "a"
    10.times { |i| a += a * i }
    GC.start
    assert GC::Tracker.start_record
    a = "a"
    10.times { |i| a += a * i }
    cycles_c, duration_c, allocs_c, ctx_switches = GC::Tracker.end_record
    cycles_p, duration_p, allocs_p, ctx_switches = GC::Tracker.end_record
    assert cycles_p > cycles_c
    assert duration_p > duration_c
    assert allocs_p > allocs_c
    assert ctx_switches
  ensure
    GC::Tracker.disable
  end

  def test_no_data_when_disabled
    assert !GC::Tracker.start_record
    assert GC::Tracker.end_record.nil?
  end

  def test_enabled_returns_accrodingly
    assert !GC::Tracker.enabled?
    GC::Tracker.enable
    assert GC::Tracker.enabled?
  ensure
    GC::Tracker.disable
  end

  def test_data_for_started_records_on_disable
    assert GC::Tracker.enable
    assert GC::Tracker.start_record
    assert GC::Tracker.disable
    assert !GC::Tracker.start_record
    assert !GC::Tracker.end_record.nil?
    assert GC::Tracker.end_record.nil?
  ensure
    GC::Tracker.disable
  end

  def test_allocations
    assert GC::Tracker.enable
    assert GC::Tracker.start_record
    omg = "10" * 10
    _cycles, _duration, allocs, _ctx_switches = GC::Tracker.end_record
    assert allocs > 10
    assert GC::Tracker.disable
  end
end

