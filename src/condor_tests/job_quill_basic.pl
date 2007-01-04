#! /usr/bin/env perl
use CondorTest;

$cmd      = 'job_quill_basic.cmd';
$testname = 'Verify quill basics';

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my $alreadydone=0;

my $submithandled = "no";
$submitted = sub
{
};



CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	print "$testname: SUCCESS\n";
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

