#!/usr/bin/env perl

use strict;
use warnings;

if (@ARGV != 1 ) {
    exit -1;
}

my $temp_file = $ARGV[0];

print "Waiting for $temp_file";

# Wait for file to be created
while(!-e $temp_file) {
    print ".";
    sleep 5;
}
print "\nFound $temp_file\n";

open(FILE, '<', "$temp_file") or die("Cannot open $temp_file for reading: $!");

# Check if file has expected contents
while (<FILE>) {
    chomp;
    print "Line: '$_'\n";
    if($_ eq "ssh_to_job_success") {
        exit 0;
    }
}

exit -1;
