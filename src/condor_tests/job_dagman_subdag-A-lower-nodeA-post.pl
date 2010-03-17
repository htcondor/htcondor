#! /usr/bin/env perl

if ($ARGV[0] eq 0) {
	print "Node A post script succeeded\n";
} else {
	print "Node A post script failed\n";
	exit 1;
}
