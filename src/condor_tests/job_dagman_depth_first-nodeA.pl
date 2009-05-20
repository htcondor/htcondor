#! /usr/bin/env perl

my $file = "job_dagman_depth_first.order";
if (-e $file) {
	unlink $file;
}
