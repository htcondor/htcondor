#!/usr/bin/env perl

use strict;

# use like this: x_renamebypattern.pl \*trigger\*

my $pattern = $ARGV[0];

#open(LOG,">run_test_log");
my $start = localtime;
print LOG "Started at $start\n";
open(TESTS,"ls $pattern | ");
while(<TESTS>)
{
	chomp;
	my $now = localtime;
	my $filebase = "";
	my $currentfile = $_;
	my $tempfile = $currentfile . ".new";

	#print LOG "fixing $_ at $now\n";
	print "fixing $currentfile at $now\n";
	#print "New file is $tempfile\n";

	if( $currentfile =~ /^(.*)(willtrigger)(.*)$/ )
	{
		print "Change to $1true$3\n"; 
		system("x_renametool.pl $currentfile $1true$3");
	}
	elsif($currentfile =~ /^(.*)(notrigger)(.*)$/ )
	{
		print "Change to $1false$3\n"; 
		system("x_renametool.pl $currentfile $1false$3");
	}
}
my $ended = localtime;
print LOG "Ended at $ended\n";
