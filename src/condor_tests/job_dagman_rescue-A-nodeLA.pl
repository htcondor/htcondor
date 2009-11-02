#! /usr/bin/env perl

my $file = "job_dagman_rescue-A-nodeLA.works";

if (-e $file) {
	unlink $file or die "Unlink failed: $!";
	print "nodeLA succeeded\n";
} else {
	system("touch $file");
	print "nodeLA failed\n";
	exit 1;
}
