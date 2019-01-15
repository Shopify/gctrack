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

  def test_records_allocations
    assert GC::Tracker.enable
    assert GC::Tracker.start_record
    _, _, first_alloc = GC::Tracker.end_record
    assert GC::Tracker.start_record
    _ = [1, 2, 3] # the values shouldn't allocate anything
    _, _, second_alloc = GC::Tracker.end_record
    assert second_alloc == first_alloc + 1
  ensure
    GC::Tracker.disable
  end

  def test_enabled_generates_events
    assert GC::Tracker.enable
    assert GC::Tracker.start_record
    GC.start
    cycles, duration, allocations = GC::Tracker.end_record
    assert cycles > 0
    assert duration > 0
    assert allocations > 0
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
    cycles_c, duration_c, allocations_c = GC::Tracker.end_record
    _ = [1, 2, 3]
    cycles_p, duration_p, allocations_p = GC::Tracker.end_record
    assert cycles_p > cycles_c
    assert duration_p > duration_c
    assert allocations_p == allocations_c * 2 + 1
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
end

