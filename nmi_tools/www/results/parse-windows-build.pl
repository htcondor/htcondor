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

my @output;
my @chunk;
my $begin_re = qr/^------ /;
my $end_re   = qw/ error\(s\),/;
while(<FILE>) {
    if(/\d+\s+succeeded/) {
        # We want this to print first, so make sure it's at the beginning of @output
        unshift @output, $_;
    }
    elsif(/$begin_re/ .. /$end_re/) {
	push @chunk, $_;

	if(/$end_re/) {
	    if(/\s0\s+error\(s\),/) {
		if(/,\s+0\s+warning/) {
                    # TJ requested that we do not print this line.  So for now, do nothing
		    # print $_;
		}
		else {
		    $_ =~ s/,\s+(\d+\s+warning\(s\))/, <font style="background-color:yellow">$1<\/font>/;
		    push @output, $_;
		}
	    }
	    else {
		pop @chunk;

                my @tmp = map { s/(error \S+)/<font style="background-color:#ff00ff">$1<\/font>/; $_ } grep / (error|warning) (C|LNK|PRJ)\d+/, @chunk;
                push @output, @tmp;

		
		$_ =~ s/\s(\d+\s+error\(s\))/ <font style="background-color:red">$1<\/font>/;
		push @output, $_;
	    }
	    @chunk = ();
	}
    }
}

print @output;
