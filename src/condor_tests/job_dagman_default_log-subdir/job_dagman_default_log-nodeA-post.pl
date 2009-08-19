#! /usr/bin/env perl

if ($ARGV[0]) {
	print "Node A post script failed\n";
} else {
	print "Node A post script succeeded\n";
}

exit($ARGV[0]);
