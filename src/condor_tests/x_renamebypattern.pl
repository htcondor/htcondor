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

# use like this: x_renamebypattern.pl \*trigger\*

my $pattern = $ARGV[0];

#open(LOG,">run_test_log");
my $start = localtime;
print LOG "Started at $start\n";
open(TESTS,"ls $pattern | ");
while(<TESTS>)
{
	CondorTest::fullchomp($_);
	my $now = localtime;
	my $filebase = "";
	my $currentfile = $_;
	my $tempfile = $currentfile . ".new";

	#print LOG "fixing $_ at $now\n";
	CondorTest::debug("fixing $currentfile at $now\n",1);
	#print "New file is $tempfile\n";

	if( $currentfile =~ /^(.*)(willtrigger)(.*)$/ )
	{
		CondorTest::debug("Change to $1true$3\n",1);
		system("x_renametool.pl $currentfile $1true$3");
	}
	elsif($currentfile =~ /^(.*)(notrigger)(.*)$/ )
	{
		CondorTest::debug("Change to $1false$3\n",1);
		system("x_renametool.pl $currentfile $1false$3");
	}
}
my $ended = localtime;
print LOG "Ended at $ended\n";
