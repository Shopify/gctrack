require_relative '../lib/gctrack/gctrack'
require 'benchmark'

def create_garbage(and_gc: false) 
  a = "a"
  10.times { |i| a += a * i }
  GC.start if and_gc
end

def test(width: 50, depth: 300)
  GC::Tracker.start_record
  width.times do 
    GC::Tracker.start_record
    create_garbage(and_gc: true)
    depth.times do 
      GC::Tracker.start_record 
      create_garbage
    end
    depth.times { GC::Tracker.end_record }
    GC::Tracker.end_record
  end
  GC::Tracker.end_record
end

raise "Couldn't enable GC::Tracker!" unless GC::Tracker.enable
3.times do
  GC::Tracker.start_record
  puts Benchmark.measure { test() }
  cycles, duration, allocs = GC::Tracker.end_record
  puts "For #{cycles} GC Cycles summing to #{(duration.to_f / 1_000_000).round(3)}ms while we allocated #{allocs} objects!"
end
