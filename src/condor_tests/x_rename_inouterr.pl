#!/usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

use CondorTest;

use strict;

#open(LOG,">run_test_log");
my $start = localtime;
print LOG "Started at $start\n";
open(TESTS,"ls *.cmd | ");
while(<TESTS>)
{
	CondorTest::fullchomp($_);
	my $now = localtime;
	my $filebase = "";
	my $currentfile = $_;
	my $tempfile = $currentfile . ".new";

	# pull the base name
	if( $currentfile =~ /(.*)\.(.*)/ )
	{
		$filebase = $1;
		CondorTest::debug("base is $filebase extension of file is $2\n",1);
	}

	#print LOG "fixing $_ at $now\n";
	CondorTest::debug("fixing $currentfile at $now\n",1);
	CondorTest::debug("New file is $tempfile\n",1);

	open(FIXIT,"<$currentfile") || die "Can't seem to open it.........$!\n";
	open(NEW,">$tempfile") || die "Can not open new file!: $!\n";
	my $line = "";
	while(<FIXIT>)
	{
		CondorTest::fullchomp($_);
		$line = $_;
		CondorTest::debug("Current line: ----$line----\n",1);
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
