[![Gem Version](https://badge.fury.io/rb/gctrack.svg)](https://badge.fury.io/rb/gctrack)
[![Build Status](https://travis-ci.org/Shopify/gctrack.svg?branch=master)](https://travis-ci.org/Shopify/gctrack)

## GCTrack

The GCTrack extension lets you monitor incremental Garbage Collector (GC) information, the GC introduced in Ruby v2.2.

### Getting started

#### #1 Add the dependency

GCTrack is published to [rubygems.org](https://rubygems.org/gems/gctrack). 
Simply add it as a dependency to your `Gemfile`

```ruby
gem 'gctrack', '~> 0.1.0'
```

#### #2 Enable the extension

```ruby
require 'gctrack'

GC::Tracker.enable # returns true, if the Tracker is now enabled
```

#### #3 Record data

```ruby
GC::Tracker.start_record # returns true, if a new record was started
# DO ACTUAL WORK
gc_cycles, gc_duration_ns = GC::Tracker.end_record
```

`#end_record` will return the `gc_cycles` (the amount of gc cycles observed) and `gc_duration_ns` (their cumulative duration in 
nanoseconds), since the last invocation of `#start_record`. You can also invoke `#start_record` recursively as so:

```ruby
GC::Tracker.start_record # Start a first record 
do_work_on(one)
GC::Tracker.start_record # Start a second record 
do_work_on(two)
inner_data = GC::Tracker.end_record # Collect results from second record
do_work_on(three)
outter_data = GC::Tracker.end_record # Collect results from first record
```

In this example, the Array `inner_data` will only contain the GC information resulted of executing `do_work_on(two)`, while 
`outter_data` will contain the GC information resulted from the three exectutions: `do_work_on` `one`, `two` and `three`. 

Effectively, records are stacked and data from a GC cycle will be added to all "currently" records.

## License

[The MIT License (MIT)](LICENSE.md)
