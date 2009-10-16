#! /usr/bin/env perl

$nodename = shift;

my $file = "job_dagman_rescue_recov-$nodename.works";

if (-e $file) {
	unlink $file or die "Unlink failed: $!";
	print "$nodename succeeded\n";
} else {
	system("touch $file");
	print "$nodename failed\n";
	exit 1;
}
