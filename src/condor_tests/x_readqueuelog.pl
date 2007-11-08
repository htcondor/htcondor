#! /usr/bin/env perl
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

my $log = $ARGV[0];
my $option = $ARGV[1];
#my $limit = 2;
my $limit = $ARGV[2];
my $count = 0;
my $line;

#print "Log is $log, Option is $option and limit is $limit\n";

open(OLDOUT, "<$log");
while(<OLDOUT>)
{
	CondorTest::fullchomp($_);
	$line = $_;
	#print "--$line--\n";
	if( $line =~ /^\s*(hello)\s*$/ )
	{
		$count++;
		#print "Count now $count\n";
	}
}

close(OLDOUT);

if($count != $limit)
{
	print "$count";
}
else
{
	print "0\n";
}

exit(0);
