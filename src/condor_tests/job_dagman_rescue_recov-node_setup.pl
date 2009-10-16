#! /usr/bin/env perl

# Set things up for lower-level nodes...

unlink <job_dagman_rescue_recov-node*.works>;

# Lower-level nodeB1 should work the first time through.
system("touch job_dagman_rescue_recov-nodeB1.works");

# Lower-level nodeC should work the first time through.
system("touch job_dagman_rescue_recov-nodeC.works");
