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

#$cmd = 'lib_procapi_cputracking-snapshot.cmd';
$cmd = $ARGV[0];

$testdesc =  'proper reporting of remote cpu usage - vanilla U';
$testname = "lib_procapi_cputracking-snapshot";
$datafile = "lib_procapi_cputracking-snapshot.data.log";
$worked = "yes";

$execute = sub
{
	CondorTest::debug( "Goood, job is running so we'll start the timer....\n",1);
};

$timed = sub
{
        print scalar localtime() . "\n";
        CondorTest::debug("lib_procapi_cpuutracking-snapshot HUNG !!!!!!!!!!!!!!!!!!\n",1);
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
			CoondorTest::debug( "usage $1\n",1);
			$counter = $counter + 1;
			$total = $total + $1;
		}
		CondorTest::debug( "$line\n",1);
	}
	close(PIN);

	if($counter != 7) {
		CondorTest::debug("WARNING: expecting 7 data values and got $counter\n",1);
	}

	CondorTest::debug("Total usage reported by children is  $total\n",1);

	my @adarray;
	my $spoolloc;
	my $status = 1;
	my $cmd = "condor_config_val spool";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
		exit(1)
	}

	foreach my $line (@adarray)
	{
		chomp($line);
		$spoolloc = $line;
	}

	CondorTest::debug("Spool directory is \"$spoolloc\"\n",1);

	#my $historyfile = $spoolloc . "/history";
	my $historyfile = `condor_config_val HISTORY`;
	chomp($historyfile);
	my $remcpu = 0.0;
	my $remwallclock = 0.0;

	CondorTest::debug("Spool history file is \"$historyfile\"\n",1);

	# look for RemoteWallClockTime and RemoteUserCpu and comparee
	# them to our calculated results

	system("cat lib_procapi_cputracking-snapshot.data.log");

	CondorTest::debug("Possibly waiting on history file \"$historyfile\":",1);
	# make sure history file has been written yet
	while(!(-f $historyfile)) {
		CondorTest::debug(".",1);
		sleep 3;
	}
	CondorTest::debug("\n",1);

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
			CondorTest::debug("ERROR: RUCpu missing: $line",1);
		}
		close(HIST);
		die "Failed to find RemoteUserCpu valaid value\n";
	}
	$status = 1;
	$cmd = "condor_config_val PID_SNAPSHOT_INTERVAL";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status)
	{
		CondorTest::debug("Test failure due to Condor Tool Failure<$cmd>\n",1);
		exit(1)
	}

	my $snapshot = 0;
	foreach $findx (@adarray) {
		$snapshot = $findx;
	}

	$low = $total - (2 + (2 * $snapshot));
	# so what range do we insist on for the test to pass???
	if($low > $remcpu) {
		CondorTest::debug("Failed: Tasks totaled to $total subtract snapshot interval(($snapshot * 2) + 2)\n",1);
		die "We wanted $remcpu in the range of  $low to $total\n";
	} else {
		CondorTest::debug("Good: Tasks totaled to $total subtract snapshot interval(($snapshot * 2) + 2)\n",1);
		CondorTest::debug("This Reported $remcpu which is in range of $low to $total\n",1);
		exit(0);
	}

};

CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );
CondorTest::RegisterExecute($testname, $execute);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

