#! /usr/bin/env perl
use CondorTest;

$cmd      = 'job_schedd_restart-holdjobs-ok.cmd';
$testname = 'Verify held jobs are preserved on schedd restarts';

$beforequeue = "job_schedd_restart-holdjobs.before";
$afterqueue = "job_schedd_restart-holdjobs.after";

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my $alreadydone=0;

my $submithandled = "no";
$submitted = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};
	my $doneyet = "no";

	if($submithandled eq "no") {
		$submithandled = "yes";

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

		# Now turn off the scheduler
		print "Turning off the scheduler\n";
		my @bdarray;
		$status = 1;
		$cmd = "condor_off -schedd";
		$status = CondorTest::runCondorTool($cmd,\@bdarray,2);
		if(!$status)
		{
			print "Test failure due to Condor Tool Failure<$cmd>\n";
			exit(1)
		}

		print "Verify scheduler off\n";
		# verify schedd is off
		$doneyet = "no";
		$stillrunning = "yes";
		while($doneyet eq "no") {
			my @cdarray;
			print "Looking for no scheduler running\n";
			$status = 1;
			$cmd = "condor_status -schedd";
			$stillrunning = "no";
			$status = CondorTest::runCondorTool($cmd,\@cdarray,2);
			if(!$status)
			{
				print "Test failure due to Condor Tool Failure<$cmd>\n";
				exit(1)
			}
			foreach my $line (@cdarray)
			{
				print "<<$line>>\n";
				if( $line =~ /.*Total.*/ ) {
					# hmmmm not done yet... scheduler still responding
					print "Schedd still running\n";
					$stillrunning = "yes";
				}
			}
			if($stillrunning eq "no") {
				print "Must be done\n";
				$doneyet = "yes";
			}
		}

		# Now turn on the scheduler
		print "Turning on the scheduler\n";
		my @ddarray;
		$status = 1;
		$cmd = "condor_on -schedd";
		$status = CondorTest::runCondorTool($cmd,\@ddarray,2);
		if(!$status)
		{
			print "Test failure due to Condor Tool Failure<$cmd>\n";
			exit(1)
		}

		print "Verify scheduler on\n";
		# verify schedd is on
		$doneyet = "no";
		my $runningyet = "no";
		while($doneyet eq "no") {
			my @edarray;
			print "Looking for scheduler running\n";
			$status = 1;
			$cmd = "condor_status -schedd";
			$runningyet = "no";
			$status = CondorTest::runCondorTool($cmd,\@edarray,2);
			if(!$status)
			{
				print "Test failure due to Condor Tool Failure<$cmd>\n";
				exit(1)
			}
			foreach my $line (@edarray)
			{
				print "<<$line>>\n";
				if( $line =~ /.*Total.*/ ) {
					# hmmmm done .. scheduler responding
					print "Schedd running now\n";
					$runningyet = "yes";
				}
			}

			if($runningyet eq "yes") {
				print "Must be done\n";
				$doneyet = "yes";
			}
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
			elsif($after =~ /^(\d+\.\d+)\s+([\w\-]+)\s+(\d+\/\d+)\s+(\d+:\d+)\s+(\d+\+\d+:\d+:\d+)\s+([HRI]+)\s+(\d+)\s+(\d+\.\d+)\s+(.*)$/) {
				#print "AF:$1 $2 $3 $4 $5 $6 $7 $8 $9\n";
				$afterstate = $6;
				if($before =~ /^(\d+\.\d+)\s+([\w\-]+)\s+(\d+\/\d+)\s+(\d+:\d+)\s+(\d+\+\d+:\d+:\d+)\s+([HRI]+)\s+(\d+)\s+(\d+\.\d+)\s+(.*)$/) {
					#print "BF:$1 $2 $3 $4 $5 $6 $7 $8 $9\n";
					$beforestate = $6;
				} else {
					die "jobs in after restart of schedd do not parse\n";
				}
				if(($afterstate ne $beforestate) || ($after ne $before)) {
					print "Before:$before(State:$beforestate)\n";
					print "After:$after(State:$afterstate)\n";
					die "Job queue was not maintained!\n";
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



CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

