#! /usr/bin/env perl

# Set things up for lower-level nodes...

# Lower-level nodeA should work the first time through.
my $file = "job_dagman_rescue-A-nodeLA.works";
if (! -e $file) {
	system("touch $file");
}

# Lower-level nodeD should fail the first time through.
my $file = "job_dagman_rescue-A-nodeD.works";

if (-e $file) {
	unlink $file or die "Unlink failed: $!";
	print "Node UA succeeded\n";
}
