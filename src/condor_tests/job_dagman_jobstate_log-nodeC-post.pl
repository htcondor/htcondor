#! /usr/bin/env perl

print "$ARGV[0] post script\n";

if ($ARGV[1] eq 0) {
	print "Failure\n";
	exit(1);
} else {
	print "Success\n";
}
