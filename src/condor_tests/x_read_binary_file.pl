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


my $input = $ARGV[0];

#print "Want to read $input\n";

if( ! -e $input )
{
	die "can not read file $input\n";
}

my $stat = sysopen( INPUT, $input, O_RDONLY);

if(!$stat)
{
	die "Could not open $input\n";
}
else
{
	#print "file open now.....\n";
}

my $buffer;
my $count = 0;
my $readcnt = 1;
my $total;

while( $readcnt != 0 )
{
	$readcnt = sysread( INPUT, $buffer, 256 );
	$total = $total + $readcnt;
	#print "Tried to read 256 and actually read $readcnt($total)\n";
}

#print "file is $total bytes large\n";
exit(0);
