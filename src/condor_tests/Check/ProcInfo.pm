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

package ProcInfo;

use CondorTest;

$submitted = sub
{
	CondorTest::debug("CpuAffinity Job submitted\n",1);
};

$aborted = sub 
{
	die "Abort event NOT expected\n";
};

$dummy = sub
{
};

$ExitSuccess = sub {
	CondorTest::debug("CpuAffinity Job completed\n",1);
};

################################################
##
## x_getmyprocinfo.pl can takes two args as a job and uses the first
## as the files to get in /proc { ie /proc/pid/file } OR if the second
## arg is present, it filters file by pattern. This test wants a third
## arg for number of bits representing cores it can run on and cores
## to request.
##
## We need to be careful if we hit a single core machine and simply
## pass the test. First request will be -1, cpu affinity will be off
## and we will look for more then one core allowed in the mask
## and we will return the cores we see. If we only see one core
## the test will pass and we will not check cpu affinity on one 
## and on two cores
##
################################################

sub RunAffinityCheck
{
    my %args = @_;
    my $testname = $args{test_name} || CondorTest::GetDefaultTestName();
    my $universe = $args{universe} || "vanilla";
    my $user_log = $args{user_log} || CondorTest::TempFileName("$testname.user_log");
	my $countcores = 0;

	if(!(exists $args{proc_file})) {
		die "Must have the name of the /proc file: proc_info not set.\n";
	}

	if(!(exists $args{proc_file_pattern})) {
		die "Must have the name of the /proc file pattern: proc_file_pattern not set.\n";
	}

	if(!(exists $args{cores_desired})) {
		die "Must have desired core count: cores_desired set.\n";
	}

	$countcores = $args{cores_desired};
	if($countcores < 0) {
		$args{cores_desired} = 1;
	}

    CondorTest::RegisterAbort( $testname, $aborted );
    CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );
    #CondorTest::RegisterExecute($testname, $execute_fn);
    #CondorTest::RegisterULog($testname, $ulog_fn);
    CondorTest::RegisterSubmit( $testname, $submitted );

    my $submit_fname = CondorTest::TempFileName("$testname.submit");
    open( SUBMIT, ">$submit_fname" ) || die "error writing to $submit_fname: $!\n";
    print SUBMIT "universe = $universe\n";
    print SUBMIT "executable = x_getmyprocinfo.pl\n";
    print SUBMIT "log = $user_log\n";
    print SUBMIT "output = cpuinfo.out\n";
	print SUBMIT "request_cpus = $args{cores_desired}\n";
	my $requestedbits = 0;
	$requestedbits = $args{cores_desired};
    print SUBMIT "arguments = $args{proc_file} $args{proc_file_pattern}\n";
    print SUBMIT "notification = never\n";
    print SUBMIT "queue\n";
    close( SUBMIT );

	print "Submitting affinity test\n";
	system("cat $submit_fname");
    my $result = CondorTest::RunTest($testname, $submit_fname, 0);

	# get Cpus_allowed line aand send for processing
	my $reportedresult = 1;
	my $bitcount = 0;
	my @output = `cat cpuinfo.out`;
	my $count = @output; # expect one line
	if( $count != 1) {
		print "Proc Return************************************************************\n";
		system("cat cpuinfo.out");
		print "Proc Return************************************************************\n";
		die "Expected a single liine of output not $count\n";
	}
	chomp($output[0]);
	$bitcount = ProcessCpus($output[0]);
	print "$bitcount returned from ProcessCpus\n";
	if($countcores < 0) {
		if($bitcount > 1) {
			# we are good return count
		} elsif($bitcount == 1) {
			$result = 1;
			print "PASSING only because we have a single core. test meaningless\n";
		} else {
			$reportedresult = 0;
			print "Crazy bit count <$bitcount> for <$output[0]>\n";
		}
	} else {
		# count must equal reques
		if($bitcount != $requestedbits) {
			print "Expected $requestedbits Got $bitcount!\n";
			$reportedresult = 0;
		}
	}

    CondorTest::RegisterResult( $reportedresult, %args );
    return $bitcount;
}



################################################
##
## There are two possible entries styles. sl5 seems to encode Cpus_alowed_List
## within the Cpus_Allowed field
##
## sl5 8 core: Cpus_allowed:	00000000,00000000,00000000,00000000,00000000,00000000,00000000,000000ff
## sl6 8 core: Cpus_allowed:	ff
##             Cpus_allowed_list:	0-7
################################################

sub ProcessCpus
{
	my $info = shift;
	my $returncount = 0;
	my $partialcount = 0;
	my @masks = ();

	print "ProcessCpus <$info>\n";
	if($info =~ /^\s*Cpus_allowed:\s+([\da-fA-F]+,.*)$/) {
		print "Process list of masks: $1\n";
		@masks  = split /,/, $1;
		foreach my $bits (@masks) {
			$partialcount = 0;
			$partialcount = CountBits($bits);
			$returncount += $partialcount;
		}
	} elsif($info =~ /^\s*Cpus_allowed:\s+([\da-fA-F]+)\s*$/) {
		print "Process single mask: $1\n";
		$returncount = CountBits($1);
	# Only hex digits a-f
	} elsif($info =~ /^\s*Cpus_allowed:\s+([a-fA-F]+,.*)$/) {
		print "Process list of masks: $1\n";
		@masks  = split /,/, $1;
		foreach my $bits (@masks) {
			$partialcount = 0;
			$partialcount = CountBits($bits);
			$returncount += $partialcount;
		}
	} elsif($info =~ /^\s*Cpus_allowed:\s+([a-fA-F]+)\s*$/) {
		print "Process single mask: $1\n";
		$returncount = CountBits($1);
	} else {
		die "Cpus_allowed: pattern unexpected:<$info>\n";
	}
	print "ProcessCpus returning <$returncount>\n";
	return($returncount);
}

sub CountBits
{
	my $mask = shift;
	my $hexmask = 0;
	$hexmask = hex $mask ;
	my $count = 0;

	while($hexmask != 0) {
		$bit = 0;
		if($hexmask &  01) {
			$count += 1;
		}
		$hexmask = $hexmask >> 1;
	}
	print "CountBits returning <$count>\n";
	return( $count );
}

1;
