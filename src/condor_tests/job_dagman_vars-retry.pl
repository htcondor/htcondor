#! /usr/bin/env perl

foreach $arg (@ARGV) {
	print "<$arg> ";
}
print "\n";

if ($ARGV[1] ne $ARGV[2]) {
	print "Mismatch between VARS value and classad value\n";
	exit(2);
}

# Only succeed on third retry.
if ($ARGV[1] < 3) {
	print "Node $ARGV[0] fails to force retry\n";
	exit(1);
} else {
	print "Node $ARGV[0] succeeds\n";
}
