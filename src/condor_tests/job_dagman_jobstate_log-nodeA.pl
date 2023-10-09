#! /usr/bin/env perl

# Get Passed proc id and sleep for different times
# to gaurantee the removal of the second job
$proc = $ARGV[0];
if ($proc > 0) { sleep(10); }

print "Node A job failing...\n";

exit(2);
