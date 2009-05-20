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


#use strict;
use Cwd;

my $pid;
my $envvar;

my $opsys = $ARGV[0];
my $sleeptime = $ARGV[1];
my $testname =  $ARGV[2];

print scalar localtime() . "\n";

print "OPSYS = $opsys\n";
print "SLEEP = $sleeptime\n";

my $path;

$path = getcwd();
my $pout = "$path/parent";
my $cout = "$path/child";
my $count;
my $innercount = 0;
my $innerpid = 0;

# parent shouldn't wait for any children
#$SIG{'CHLD'} = "IGNORE";


system("rm -rf $testname.data.kids");

open(POUT, "> $testname.data") or die "Could not open file '$testname.data': $?";
print POUT "Parent's pid is $$\n\n";

$count = 0;
while ($count < 3) 
{
	print "Main loop counter at $count\n";
	$pid = fork();
	$innercount = 0;
	if ($pid == 0)
	{
		# child code....

		# child waits until a signal shows up
		#$SIG{'INT'} = \&handler;
		#$SIG{'HUP'} = \&handler;
		#$SIG{'TERM'} = \&handler;

		close(POUT);

		while ($innercount < 1) 
		{
			print "Innerloop counter at $innercount\n";
			$innerpid = fork();
			if ($innerpid == 0)
			{
				# child code....
		
				# child waits until a signal shows up
				#$SIG{'INT'} = \&handler;
				#$SIG{'HUP'} = \&handler;
				#$SIG{'TERM'} = \&handler;
	
				while (1) { sleep 1; }
	
				exit 1;
			}
			else
			{
#
				# parent code...
	
				open(GCPOUT, ">> $testname.data.kids") or die "Could not open file '$testname.data.kids': $?";
				if (!defined($innerpid)) {
					print GCPOUT "Grand kid fork failed!!!!!!!!!!!!!!!!!!!\n";
					die "some problem forking. Oh well.\n";
				}

				print GCPOUT "Relationship: $$ child created $innerpid\n";
				close(GCPOUT);
				sleep 1;
	
				$innercount++;
			}
		}

		while (1) { sleep 1; }

		exit 1;
	}
	else
	{

		# parent code...

		if (!defined($pid)) {
			die "some problem forking. Oh well.\n";
		}
	
		print POUT "Relationship: $$ created $pid\n";

		$count++;
	}
}

#print POUT "Parent's environment is:\n";
#foreach $envvar (sort keys (%ENV)) {
	#print POUT "$envvar = $ENV{$envvar}\n";
#}

close(POUT);
sleep $sleeptime;
print scalar localtime() . "\n";

exit 0;

sub handler
{
	my $sig = shift(@_);

	open(COUT, "> ${cout}_$$") or die "Could not open file '${cout}_$$': $?";
	print COUT "Child's pid is $$\n";
	print COUT "Got signal $sig\n\n";
	print COUT "Child's environment is:\n";
	foreach $envvar (sort keys (%ENV)) {
		print COUT "$envvar = $ENV{$envvar}\n";
	}
	close(COUT);

	exit 0;
}
