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



my $arg = $ARGV[0];

my $cfig = $ENV{CONDOR_CONFIG};
my $pfig = $ENV{PATH};
my $ufig = $ENV{UNIVERSE};

print "Mode is $arg\n";

print "Extracting CONDOR_CONFIG, PATH and UNIVERSE from environment\n";
print "CONDOR_CONFIG = $cfig\n";
print "PATH = $pfig\n";
print "UNIVERSE = $ufig\n";

if ( $cfig eq undef )
{
	print "{$arg}CONDOR_CONFIG is undefined\n";
	if( $arg ne "failok" ) { exit(1); }
}
else
{
	print "{$arg}CONDOR_CONFIG is $cfig\n";
	if( $arg ne "failnotok" ) { exit(1); }
}

if ( $pfig eq undef )
{
	print "{$arg}PATH is undefined\n";
	if( $arg ne "failok" ) { exit(1); }
}
else
{
	print "{$arg}PATH is $pfig\n";
	if( $arg ne "failnotok" ) { 
	 print "NMI places PATH into the environment, OK\n";
	 	exit(1); 
	 }
}

if ( $ufig eq undef )
{
	print "{$arg}UNIVERSE is undefined\n";
	if( $arg ne "failok" ) { exit(1); }
}
else
{
	print "{$arg}UNIVERSE is $ufig\n";
	if( $arg ne "failnotok" ) { exit(1); }
}

