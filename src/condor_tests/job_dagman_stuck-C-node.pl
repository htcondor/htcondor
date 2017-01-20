#! /usr/bin/env perl

print "Node $ARGV[0] is $ARGV[1].$ARGV[2]\n";
print "DAG status: $ARGV[4]\n";

if ($ARGV[2] eq $ARGV[3]) {
	$id = $ARGV[1] . "." . $ARGV[2];
	print "Doing condor_hold on $id\n";
	system("condor_hold $id");
}

# Sleep time should be greater than DAGMAN_MAX_STUCK_TIME * 60
sleep(120);

if ($ARGV[4] ne 0) {
	print "Node $ARGV[0] failing because DAG status ($ARGV[4]) is not DAG_STATUS_OK\n";
	exit(1);
}

print "Node $ARGV[0] finishing\n";
