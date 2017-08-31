require 'mkmf'

$CFLAGS = "-O3 -Wall"

create_makefile('gctrack/gctrack')
