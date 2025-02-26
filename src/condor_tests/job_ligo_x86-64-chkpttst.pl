#!/usr/bin/env perl
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

$cmd = 'job_ligo_x86-64-chkpttst.cmd';
$testdesc =  'LIGO Chkpt Test';
$testname = "job_ligo_x86-64-chkpttst";

my $tarball = "x_job_ligo_x86-64-chkpttst.tar.gz";
my $fetch_prog;
my $fetch_url = "https://htcss-downloads.chtc.wisc.edu/externals-private/$tarball";
if ( $^O eq "darwin" ) {
	$fetch_prog = "curl -O";
} else {
	# Retry up to 60 times, waiting 22 seconds between tries
	$fetch_prog = "wget --tries=60 --timeout=22";
}
CondorTest::debug("Fetching $fetch_url\n",1);
my $rc = system( "$fetch_prog $fetch_url 2>&1" );
if ($rc) {
	$status = $rc >> 8;
	CondorTest::debug("URL retrieval failed: $status\n",1);
	die "LIGO app not fetched\n";
}

$tarres = system("tar -zxvf $tarball");

CondorTest::debug("Extract LIGO test files\n",1);
CondorTest::debug("Condor compile LIGO application\n",1);
$condorize = system("./build.condor.sh");
if( !(-s "ring")) {
	die "LIGO app not built\n";
}

$hey = system("./ligo_medium_setup.sh");
CondorTest::debug("Medium set up done, now configuring to run Ring\n",1);

$ExitSuccess = sub {
	if( (-s "L1-RING-793420200-2176.xml") &&  (-s "L1-RINGBANK-793420200-2176.xml") ) {
		system("ls -l *.xml");
		CondorTest::debug("Good! These files should be current and non-zero\n",1);
		system("cat job_ligo_x86-64-chkpttst.err");
		return(0);
	} else { 
		system("ls -l *.xml");
		die "One or more of the output files is 0 sized...\n";
	}
};

# We can be more clever about this later.
$CheckpointSuccess = sub { CondorTest::AddCheckpoint(); return 0; };

CondorTest::RegisterExitedSuccess( $testname, $ExitSuccess );
CondorTest::RegisterEvictedWithCheckpoint( $testname, $CheckpointSuccess );


if( CondorTest::RunTest($testname, $cmd, 1) ) {
    CondorTest::debug("$testname: SUCCESS\n",1);
    exit(0);
} else {
    die "$testname: CondorTest::RunTest() failed\n";
}


