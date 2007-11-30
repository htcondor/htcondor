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

Condor::DebugOff();

#$cmd = 'lib_procapi_cputracking-snapshot.cmd';
$cmd = $ARGV[0];

$testname = 'proper reporting of remote cpu usage - vanilla U';
$datafile = "lib_procapi_cputracking-snapshot.data.log";
$worked = "yes";

$execute = sub
{
	Condor::debug( "Goood, job is running so we'll start the timer....\n");
};

$timed = sub
{
        system("date");
        print "lib_procapi_cpuutracking-snapshot HUNG !!!!!!!!!!!!!!!!!!\n";
        exit(1);
};

$ExitSuccess = sub {
	my %info = @_;
	my $line;
	my $pidline;
	my $counter = 0;
	my $total = 0.0;

	open(PIN, "<$datafile") or die "Could not open data file '$datafile': $?";
	while(<PIN>)
	{
		CondorTest::fullchomp();
		$line = $_;
		if( $line =~ /^\s*done\s+<(\d+\.?\d*)>\s*$/ )
		{
			Condor::debug( "usage $1\n");
			$counter = $counter + 1;
			$total = $total + $1;
		}
		Condor::debug( "$line\n");
	}
	close(PIN);

	if($counter != 7) {
		print "WARNING: expecting 7 data values and got $counter\n";
	}

	print "Total usage reported by children is  $total\n";

	my @adarray;
	my $spoolloc;
	my $status = 1;
	my $cmd = "condor_config_val spool";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1)
	}

	foreach my $line (@adarray)
	{
		chomp($line);
		$spoolloc = $line;
	}

	print "Spool directory is \"$spoolloc\"\n";

	#my $historyfile = $spoolloc . "/history";
	my $historyfile = `condor_config_val HISTORY`;
	chomp($historyfile);
	my $remcpu = 0.0;
	my $remwallclock = 0.0;

	print "Spool history file is \"$historyfile\"\n";

	# look for RemoteWallClockTime and RemoteUserCpu and comparee
	# them to our calculated results

	system("cat lib_procapi_cputracking-snapshot.data.log");

	print "Possibly waiting on history file \"$historyfile\":";
	# make sure history file has been written yet
	while(!(-f $historyfile)) {
		print ".";
		sleep 3;
	}
	print "\n";

	# let history file get finished before getting upset about missing values
	my $trys = 4;
	my $count = 0;

	while($count < $trys) {
		open(HIST,"<$historyfile") || die "Could not open historyfile:($historyfile) : !$\n";
		while(<HIST>) {
			$line  = $_;
			if($line =~ /^\s*RemoteWallClockTime\s*=\s*(\d+\.?\d*).*$/) {
				#print "Found RemoteWallClockTime = $1\n";
				$remwallclock = $1;
			} elsif($line =~ /^\s*RemoteUserCpu\s*=\s*(\d+\.?\d*).*$/) {
				#print "Found RemoteUserCpu = $1\n";
				$remcpu = $1;
			}
		}
		close(HIST);
		if($remcpu != 0) {
			last;
		}
		$count = $count + 1;
		sleep(2 * $count);
	}

	if($remcpu == 0) {
		open(HIST,"<$historyfile") || die "Could not open historyfile:($historyfile) : !$\n";
		while(<HIST>) {
			$line  = $_;
			print "ERROR: RUCpu missing: $line";
		}
		close(HIST);
		die "Failed to find RemoteUserCpu valaid value\n";
	}
	$status = 1;
	$cmd = "condor_config_val PID_SNAPSHOT_INTERVAL";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1)
	}

	my $snapshot = 0;
	foreach $findx (@adarray) {
		$snapshot = $findx;
	}

	$low = $total - (2 + (2 * $snapshot));
	# so what range do we insist on for the test to pass???
	if($low > $remcpu) {
		print "Failed: Tasks totaled to $total subtract snapshot interval(($snapshot * 2) + 2)\n";
		die "We wanted $remcpu in the range of  $low to $total\n";
	} else {
		print "Good: Tasks totaled to $total subtract snapshot interval(($snapshot * 2) + 2)\n";
		print "This Reported $remcpu which is in range of $low to $total\n";
		exit(0);
	}

};

CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );
CondorTest::RegisterExecute($testname, $execute);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

