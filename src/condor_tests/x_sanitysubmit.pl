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

my $pid = $ARGV[0];

my $testdesc =  'sanity submit test - vanilla U';
my $testname = "x_sanitysubmit";
my $cmd = "x_sanitysubmit$pid.cmd";
my $template = 'x_sanitysubmit.template';


my $line = "";
open(TEMPLATE,"<$template") or die "Can not open $template: $!\n";
open(CMD,">$cmd") or die "Can not open $cmd: $!\n";
while(<TEMPLATE>) {
	chomp();
	$line = $_;
	if($line =~ /^\s*log\s*=.*$/) {
		print CMD "log = $testname$pid.log\n";
	} elsif($line =~ /^\s*error\s*=.*$/) {
		print CMD "error = $testname$pid.err\n";
	} elsif($line =~ /^\s*output\s*=.*$/) {
		print CMD "output = $testname$pid.out\n";
	} else {
		print CMD "$line\n";
	}
}
close(TEMPLATE);
close(CMD);

CondorTest::debug("Submit file for this test is $cmd\n",1);


$aborted = sub {
	my %info = @_;
	my $done;
	die "Abort event not expected!\n";
};

$held = sub {
	my %info = @_;
	my $cluster = $info{"cluster"};
	my $holdreason = $info{"holdreason"};

	die "Held event not expected: $holdreason \n";
};

$executed = sub
{
	my %args = @_;
	my $cluster = $args{"cluster"};

};

$success = sub
{
	CondorTest::debug("Success: ok\n",1);
};

$release = sub
{
	die "Release NOT expected.........\n";
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);

if( CondorTest::RunTest($testname, $cmd, 0) ) {
	CondorTest::debug("$testname: SUCCESS\n",1);
	exit(0);
} else {
	die "$testname: CondorTest::RunTest() failed\n";
}

