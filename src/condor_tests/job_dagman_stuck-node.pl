#! /usr/bin/env perl

print "Node $ARGV[0] is $ARGV[1].$ARGV[2]\n";

if ($ARGV[2] eq $ARGV[3]) {
	$id = $ARGV[1] . "." . $ARGV[2];
	print "Doing condor_hold on $id\n";
	#TEMPTEMP -- is system the right way to do this?
	system("condor_hold $id");
}

# Sleep time should be greater than DAGMAN_MAX_STUCK_TIME * 60
sleep(120);

print "Node $ARGV[0] finishing\n";
