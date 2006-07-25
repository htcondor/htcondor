#!/usr/bin/env perl

use strict;

#open(LOG,">run_test_log");
my $start = localtime;
print LOG "Started at $start\n";
open(TESTS,"ls *.cmd | ");
while(<TESTS>)
{
	chomp;
	my $now = localtime;
	my $filebase = "";
	my $currentfile = $_;
	my $tempfile = $currentfile . ".new";

	# pull the base name
	if( $currentfile =~ /(.*)\.(.*)/ )
	{
		$filebase = $1;
		print "base is $filebase extension of file is $2\n";
	}

	#print LOG "fixing $_ at $now\n";
	print "fixing $currentfile at $now\n";
	print "New file is $tempfile\n";

	open(FIXIT,"<$currentfile") || die "Can't seem to open it.........$!\n";
	open(NEW,">$tempfile") || die "Can not open new file!: $!\n";
	my $line = "";
	while(<FIXIT>)
	{
		chomp;
		$line = $_;
		print "Current line: ----$line----\n";
		if( $line =~ /^\s*error\s*=\s*.*$/ )
		{
			print NEW "error = $filebase.err\n";
		}
		elsif( $line =~ /^\s*Error\s*=\s*.*$/ )
		{
			print NEW "error = $filebase.err\n";
		}
		elsif( $line =~ /^\s*output\s*=\s*.*$/ )
		{
			print NEW "output = $filebase.out\n";
		}
		elsif( $line =~ /^\s*Output\s*=\s*.*$/ )
		{
			print NEW "output = $filebase.out\n";
		}
		elsif( $line =~ /^\s*log\s*=\s*.*$/ )
		{
			print NEW "log = $filebase.log\n";
		}
		elsif( $line =~ /^\s*Log\s*=\s*.*$/ )
		{
			print NEW "log = $filebase.log\n";
		}
		else
		{
			print NEW "$line\n";
		}
	}
	close(FIXIT);
	close(NEW);
	system("mv $tempfile $currentfile");
}
my $ended = localtime;
print LOG "Ended at $ended\n";
