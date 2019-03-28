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
    cycles, duration, mark, sweep = GC::Tracker.end_record
    assert cycles > 0
    assert duration > 0
    assert mark == 0 # full GC!
    # assert sweep > 0
  ensure
    GC::Tracker.disable
  end

  def test_recurses
    assert GC::Tracker.enable
    GC.start
    assert GC::Tracker.start_record
    a = "a"
    10.times { |i| a += a * i }
    GC.start
    assert GC::Tracker.start_record
    100.times { |o| a = "#{o}"; 10.times { |i| a += a * i } }
    cycles_c, duration_c, mark_c, sweep_c = GC::Tracker.end_record
    GC.start
    cycles_p, duration_p, mark_p, sweep_p = GC::Tracker.end_record
    assert cycles_p > cycles_c
    assert duration_p > duration_c
    # assert mark_p > 0
    # assert sweep_p > sweep_c

    # puts "#{(duration_c.to_f / 1000000).round(3)}, #{(mark_c.to_f / 1000000).round(3)}, #{(sweep_c.to_f / 1000000).round(3)}"
    # puts "#{(duration_p.to_f / 1000000).round(3)}, #{(mark_p.to_f / 1000000).round(3)}, #{(sweep_p.to_f / 1000000).round(3)}"

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

