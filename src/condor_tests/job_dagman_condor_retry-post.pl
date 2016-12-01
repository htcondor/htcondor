#! /usr/bin/env perl

$expected_exit = $ARGV[0];
$actual_exit = $ARGV[1];

if ($expected_exit ne $actual_exit) {
	print "Failure: expected != actual\n";
	exit(1);
}
