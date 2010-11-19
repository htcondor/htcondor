#! /usr/bin/env perl

my $file = "job_dagman_jobstate_log-nodeD.fails";

if (-e $file) {
	unlink $file or die "Unlink failed: $!";

	# Sleep here so we have consistency in which events show up.
	sleep 5;

	open (OUTPUT, "condor_hold $ARGV[0] 2>&1 |") or die "Can't fork: $!";
	while (<OUTPUT>) {
		print "$_";
	}
	close (OUTPUT) or die "Condor_hold failed: $?";

	sleep 10;

	open (OUTPUT, "condor_release $ARGV[0] 2>&1 |") or die "Can't fork: $!";
	while (<OUTPUT>) {
		print "$_";
	}
	close (OUTPUT) or die "Condor_release failed: $?";

	print "Node D failed\n";
	exit(1);
} else {
	print "Node D succeeded\n";
}
