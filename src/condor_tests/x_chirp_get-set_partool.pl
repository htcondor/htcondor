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

use File::Copy;
use File::Path;
use CondorTest;
use Condor;

$node = $ARGV[0];
$condorchirp = $ARGV[1];
$waitmax = 40;

if($node eq "0") {
	sleep(90);
	print "Node 0 done sleeping\n";
} elsif($node eq "1") {
	#verbose_system("condor_chirp set_job_attr SETTEST true");
	print "Try  condor_chirp set_job_attr SETTEST true\n";
	my @adarray;
	my $status = 1;
	my $cmd = "$condorchirp set_job_attr SETTEST true";
	$status = CondorTest::runCondorTool($cmd,\@adarray,2);
	if(!$status) {
		print "Test failure due to Condor Tool Failure<$cmd>\n";
		exit(1);
	}
} elsif($node eq "2") {
	sleep(10);
	$attr = `$condorchirp set_job_attr SETTEST`;
	chomp($attr);
	printf "SETTEST is <$attr>\n";
} else {
}

sub verbose_system 
{

my @args = @_;
my $rc = 0xffff & system @args;

printf "system(%s) returned %#04x: ", @args, $rc;

	if ($rc == 0) 
	{
		print "ran with normal exit\n";
		return $rc;
	}
	elsif ($rc == 0xff00) 
	{
		print "command failed: $!\n";
		return $rc;
	}
	elsif (($rc & 0xff) == 0) 
	{
		$rc >>= 8;
		print "ran with non-zero exit status $rc\n";
		return $rc;
	}
	else 
	{
		print "ran with ";
		if ($rc &   0x80) 
		{
			$rc &= ~0x80;
			print "coredump from ";
			return $rc;
		}
		print "signal $rc\n"
	}
}

