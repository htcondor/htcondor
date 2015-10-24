#! /usr/bin/env perl

$retry = $ARGV[0];
if ($retry gt 0) {
	print "PRE script succeeds\n";
	close(OUT);
} else {
	print "PRE script fails\n";
	exit 1;
}
