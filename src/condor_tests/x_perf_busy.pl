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


my $request = $ARGV[0];
#print "Welcome to factorial tutorial! -- $request --\n";

my $start = time();
my $lapsed = 0;
my $now = 0;

my $res = 0;

while( $lapsed <= $request )
{
	$res = factorial(50000);
	$now = time();
	$lapsed = $now - $start;
}


#print "6 factorial = $res\n";

sub factorial
{
	my $arg = shift @_;

	#print "factorial called with $arg\n";
	if( $arg == 0 ) 
	{
		#print "arg 0 return 1\n";
		return(1);
	}
	if( $arg == 1 ) 
	{
		#print "arg 1 return 1\n";
		return(1);
	}
	else
	{
		return(factorial($arg - 1) * $arg);
	}
}
