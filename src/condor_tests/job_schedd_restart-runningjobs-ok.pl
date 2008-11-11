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

$cmd      = 'job_schedd_restart-runningjobs-ok.cmd';
$testname = 'Verify running jobs are preserved on schedd restarts';

$beforequeue = "job_schedd_restart-runningjobs.before";
$afterqueue = "job_schedd_restart-runningjobs.after";

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my $alreadydone=0;

my $count = 0;
$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};
	my $doneyet = "no";

	$count = $count + 1;
	if($count == 10) {

		print "submitted\n";
		print "Collecting queue details on $cluster\n";
		my @adarray;
		my $status = 1;
		my $cmd = "condor_q $cluster";
		$status = CondorTest::runCondorTool($cmd,\@adarray,2);
		if(!$status)
		{
			print "Test failure due to Condor Tool Failure<$cmd>\n";
			exit(1)
		}

		open(BEFORE,">$beforequeue") || die "Could not ope file for before stats $!\n";
		foreach my $line (@adarray)
		{
			print "$line\n";
			print BEFORE "$line\n";
		}
		close(BEFORE);

		$status = CondorTest::changeDaemonState( "schedd", "off", 9 );
		if(!$status)
		{
			print "Test failure: could not turn scheduler off!\n";
			exit(1)
		}

		$status = CondorTest::changeDaemonState( "startd", "off", 9 );
		if(!$status)
		{
			print "Test failure: could not turn scheduler off!\n";
			exit(1)
		}

		$status = CondorTest::changeDaemonState( "schedd", "on", 9 );
		if(!$status)
		{
			print "Test failure: could not turn scheduler on!\n";
			exit(1)
		}

		print "Collecting queue details on $cluster\n";
		my @fdarray;
		my $status = 1;
		my $cmd = "condor_q $cluster";
		$status = CondorTest::runCondorTool($cmd,\@fdarray,2);
		if(!$status)
		{
			print "Test failure due to Condor Tool Failure<$cmd>\n";
			exit(1)
		}

		open(AFTER,">$afterqueue") || die "Could not ope file for before stats $!\n";
		foreach my $line (@fdarray)
		{
			print "$line\n";
			print AFTER "$line\n";
		}
		close(AFTER);

		# Now compare the state of the job queue
		my $afterstate;
		my $beforestate;
		my $af1, $af3;
		my $bf1, $bf3;
		foreach $before (@adarray) {
			#print "Before:$before\n";
			$after = shift(@fdarray);
			#print "After:$after\n";
			#handle submitter line
			if($after =~ /^(-- Submitter.*<\d+\.\d+\.\d+\.\d+:)(\d+)(> : .*)$/) {
				$af = $1 . $3;
				if($before =~ /^(-- Submitter.*<\d+\.\d+\.\d+\.\d+:)(\d+)(> : .*)$/) {
					$bf = $1 . $3;
				} else {
					die "Submitter lines from before and after don't line up\n";
				}
				if($af ne $bf ) {
					die "Submitter lines from before and after don't match\n";
				}
			} 
			#handle job line
			elsif($after =~ /^(\d+\.\d+\s+[\w\-]+\s+\d+\/\d+\s+\d+:\d+\s+\d+\+\d+:\d+:\d+)\s+([HRI]+)\s+(\d+\s+\d+\.\d+\s+.*)$/) {
				print "AF:$1 $2 $3\n";
				$afterstate = $2;
				if($before =~ /^(\d+\.\d+\s+[\w\-]+\s+\d+\/\d+\s+\d+:\d+\s+\d+\+\d+:\d+:\d+)\s+([HRI]+)\s+(\d+\s+\d+\.\d+\s+.*)$/) {
					print "BF:$1 $2 $3\n";
					$beforestate = $2;
				} else {
					die "jobs in after restart of schedd do not parse\n";
				}
				if($afterstate ne $beforestate) {
					if(($af1 ne $bf1) || ($af3 ne $bf3)) {
						# want the state only to be different was R and now should be I
						print "Before:$before(State:$beforestate)\n";
						print "After:$after(State:$afterstate)\n";
						die "Job queue was not maintained!\n";
					} else {
						if(($beforestate ne "R") || ($afterstate ne "I")) {
							die "Before state must be R<<$beforestate>> and after state I<<$afterstate>>\n";
						}
						return(0);
					}
				} else {
					print "Before:$before(State:$beforestate)\n";
					print "After:$after(State:$afterstate)\n";
					die "Job queue was not maintained! Suspect job shows as running\n";
				}
			}
			#handle label line
			else {
				# don't care
			}
		}
		exit(0);
	}
};



CondorTest::RegisterExecute( $testname, $executed );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

