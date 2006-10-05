#! /usr/bin/env perl
use CondorTest;

$cmd = 'lib_auth_protocol-negot-prt2.cmd';
$testname = 'Condor submit to test security negotiations';

my $killedchosen = 0;

my %securityoptions =
(
"NEVER" => "1",
"OPTIONAL" => "1",
"PREFERRED" => "1",
"REQUIRED" => "1",
);

# truly const variables in perl
sub IDLE{1};
sub HELD{5};
sub RUNNING{2};

my %testerrors;
my %info;
my $cluster;

my $piddir = $ARGV[0];
my $subdir = $ARGV[1];
my $expectedres = "";
$expectedres = $ARGV[2];

print "Handed args from main test loop.....<<<<<<<<<<<<<<<<<<<<<<$piddir/$subdir/$expectedres>>>>>>>>>>>>>>\n";
$abnormal = sub {
	my %info = @_;

	die "Do not want to see abnormal event\n";
};

$aborted = sub {
	my $done;
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	die "Do not want to see aborted event\n";
};

$held = sub {
	my $done;
	%info = @_;
	$cluster = $info{"cluster"};
	$job = $info{"job"};

	die "Do not want to see aborted event\n";
};

$executed = sub
{
	%info = @_;
	$cluster = $info{"cluster"};

	print "Good. for on_exit_hold cluster $cluster must run first\n";
	my @adarray;
	my $status = 1;
	my $cmd = "condor_q -debug";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2,"Security");
	if(!$status) {
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1);
	} else {
		foreach $line (@adarray) {
			#print "Client: $line\n";
			if( $line =~ /.*KEYPRINTF.*/ ) {
				if( $expectedres eq "yes" ) {
					print "SWEET: client got key negotiation as expected\n";
				} elsif($expectedres eq "no") {
					print "Client: $line\n";
					print "BAD!!: client got key negotiation as NOT!!! expected\n";
				} elsif($expectedres eq "fail") {
					print "Client: $line\n";
					print "BAD!!: client got key negotiation as NOT!!! expected\n";
				}
			}
		}
	}
};

$submitted = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "submitted: \n";
	{
		print "good job $job expected submitted.\n";
	}
};

$success = sub
{
	my %info = @_;
	my $cluster = $info{"cluster"};

	print "Good completion!!!\n";
	my $found = PersonalPolicySearchLog( $piddir, $subdir, "Encryption", "SchedLog");
	print "PersonalPolicySearchLog found $found\n";
	if( $found eq $expectedres ) {
		print "Good completion!!! found $found\n";
	} else {
		die "Expected <<$expectedres>> but found <<$found>>\n";
	}

};

sub PersonalSearchLog 
{
	my $pid = shift;
	my $personal = shift;
	my $searchfor = shift;
	my $logname = shift;

	my $logloc = $pid . "/" . $pid . $personal . "/log/" . $logname;
	print "Search this log <$logloc> for <$searchfor>\n";
	open(LOG,"<$logloc") || die "Can not open logfile<$logloc>: $!\n";
	while(<LOG>) {
		if( $_ =~ /$searchfor/) {
			print "FOUND IT! $_";
			return(0);
		}
	}
	return(1);
}

sub PersonalPolicySearchLog 
{
	my $pid = shift;
	my $personal = shift;
	my $policyitem = shift;
	my $logname = shift;

	my $logloc = $pid . "/" . $pid . $personal . "/log/" . $logname;
	print "Search this log <$logloc> for <$policyitem>\n";
	open(LOG,"<$logloc") || die "Can not open logfile<$logloc>: $!\n";
	while(<LOG>) {
		if( $_ =~ /^.*Security Policy.*$/) {
			while(<LOG>) {
				if( $_ =~ /^\s*$policyitem\s*=\s*\"(\w+)\"\s*$/ ) {
					#print "FOUND IT! $1\n";
					if(!defined $securityoptions{$1}){
						print "Returning <<$1>>\n";
						return($1);
					}
				}
			}
		}
	}
	return("bad");
}

CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterExitedAbnormal( $testname, $abnormal );
CondorTest::RegisterExitedSuccess( $testname, $success );
CondorTest::RegisterAbort( $testname, $aborted );
CondorTest::RegisterHold( $testname, $held );
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	if( $expectedres eq "fail" ) {
		die "$testname: Test FAILED: ran but was supposed to fail\n";
	} else {
		print "$testname: SUCCESS\n";
		exit(0);
	}
} else {
	if( $expectedres eq "fail" ) {
		print "$testname: SUCCESS (failures were expected!)\n";
		exit(0);
	} else {
		die "$testname: CondorTest::RunTest() failed\n";
	}
}

