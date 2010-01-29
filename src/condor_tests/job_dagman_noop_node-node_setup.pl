#! /usr/bin/env perl

# Set things up for lower-level nodes...

$nodename = shift;

unlink <job_dagman_noop_node-node*.works>;
unlink <job_dagman_noop_node-subdir/job_dagman_noop_node-node*.works>;

print "$nodename succeeded\n";
