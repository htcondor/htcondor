#!/usr/bin/env perl
use strict;
use warnings;

# This script does some "smart" parsing on a Windows build output and display
# only important information (the definition of that may change over time but
# when I wrote the script it meant show all output for sections with errors
# and hide the output from any sections without errors).
#
# This script is used by Run-condor-taskdetails.php.  I'm a lot faster coding
# in perl than PHP so I made a separate script.  It outputs HTML that the PHP
# script can embed directly.

if(@ARGV != 1) {
    print "ERROR: $0 was called incorrectly.\n";
    print "Usage: $0 <file to parse>\n";
    exit 1;
}

# Strip off characters which could change how we open the file
$ARGV[0] =~ s/[|<>]//g;

open(FILE, '<', $ARGV[0]) or die("Cannot open $ARGV[0]: $!");

my @chunk;
my $begin_re = qr/^------ /;
my $end_re   = qw/ error\(s\),/;
while(<FILE>) {
    if(/$begin_re/ .. /$end_re/) {
	push @chunk, $_;

	if(/$end_re/) {
	    if(/\s0\s+error\(s\),/) {
		if(/,\s+0\s+warning/) {
		    print $_;
		}
		else {
		    $_ =~ s/,\s+(\d+\s+warning\(s\))/, <font style="background-color:yellow">$1<\/font>/;
		    print $_;
		}
	    }
	    else {
		pop @chunk;
		print @chunk;
		
		$_ =~ s/\s(\d+\s+error\(s\))/ <font style="background-color:red">$1<\/font>/;
		print $_;
	    }
	    @chunk = ();
	}
    }
}
