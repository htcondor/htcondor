#!/usr/bin/env perl
##
## PERIOIDIC_HOLD - False
## Check to make sure that the job never goes on hold
##
use CondorTest;

$cmd = 'job_core_perhold-false_local.cmd';
$testname = 'Condor submit policy test for PERIOIDIC_HOLD - local U';

my %info;
my $cluster;

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

##
## executed
## Just announce that the job began execution
##
$executed = sub {
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	print "Good - Job $cluster.$job began execution.\n";
};

##
## held
## If the job went on hold, we need to abort
##
$held = sub {
	my $done;
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};
	print "Bad - Job $cluster.$job should not be on hold.\n";
	exit(1);
};

##
## success
##
$success = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $job = $info{"job"};
	
	##
	## This probably isn't necessary, but just ot be safe we need to
	## check the status of the job and if it's on hold, call
	## the held() method
	##	
	if ( CondorTest::getJobStatus($cluster) == HELD ) {
		&$held( %info ) if defined $held;
		return;
	}
	print "Good - Job $cluster.$job finished executing and exited.\n";
	print "Policy Test Completed\n";
};

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedSuccess( $testname, $success );
CondorTest::RegisterHold( $testname, $held );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}
