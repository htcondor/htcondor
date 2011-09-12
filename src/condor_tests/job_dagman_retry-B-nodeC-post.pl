#! /usr/bin/env perl

$return = $ARGV[0];
if ($return eq 0) {
	print "POST script succeeds\n";
	close(OUT);
} else {
	print "POST script fails\n";
	exit 1;
}
