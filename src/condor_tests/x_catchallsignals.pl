#! /usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

use strict;
use warnings;
use Cwd;

my $dbinstalllog =  "catchallsignals.out";
#print "Trying to open logfile... $dbinstalllog\n";
#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">$dbinstalllog") or die "Could not open $dbinstalllog: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR);
 #$| = 1;
 #select(STDOUT);
  $| = 1;

# Set an alarm
sub sigterm_handler()
{
	system("date");
	print "Caught sigterm; ignoring \n";
};

sub sigint_handler()
{
	system("date");
	print "Caught sigint; ignoring \n";
};
sub handler()
{
	#print "Caught signal; ignoring \n";
};

$SIG{TERM} = \&sigterm_handler;
$SIG{INT} = \&sigint_handler;
$SIG{QUIT} = \&handler;
$SIG{HUP} = \&handler;
$SIG{ILL} = \&handler;
$SIG{TRAP} = \&handler;
$SIG{ABRT} = \&handler;
$SIG{BUS} = \&handler;
$SIG{FPE} = \&handler;
$SIG{USR1} = \&handler;
$SIG{SEGV} = \&handler;
$SIG{USR2} = \&handler;
$SIG{PIPE} = \&handler;
$SIG{ALRM} = \&handler;
$SIG{CHLD} = \&handler;
$SIG{CONT} = \&handler;
$SIG{STOP} = \&handler;
$SIG{TSTP} = \&handler;
$SIG{TTIN} = \&handler;
$SIG{TTOU} = \&handler;
$SIG{URG} = \&handler;
$SIG{XCPU} = \&handler;
$SIG{XFSZ} = \&handler;
$SIG{PROF} = \&handler;
$SIG{POLL} = \&handler;
$SIG{SYS} = \&handler;
$SIG{STKFLT} = \&handler;
$SIG{WINCH} = \&handler;
$SIG{PWR} = \&handler;
$SIG{RTMIN} = \&handler;

while(1)
{
	system("date");
	sleep(1);
}

