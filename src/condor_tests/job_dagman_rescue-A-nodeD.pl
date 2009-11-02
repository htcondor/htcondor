#! /usr/bin/env perl

my $file = "job_dagman_rescue-A-nodeD.works";

if (-e $file) {
	unlink $file or die "Unlink failed: $!";
	print "nodeD succeeded\n";
} else {
	system("touch $file");
	print "nodeD failed\n";
	exit 1;
}
