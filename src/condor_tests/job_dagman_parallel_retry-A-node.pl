#! /usr/bin/env perl

if ($ARGV[0] == 0) {
	#TEMPTEMP -- increase the sleep?
	sleep(60);
	exit(0);
} else {
	exit($ARGV[0]);
}
