#!/usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2011 Condor Team, Computer Sciences Department,
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
use CondorUtils;

my $iswindows = CondorUtils::is_windows();
my $pid = fork;
if (defined $pid ) {
	if($iswindows) {
		if($pid != 0) {
			exit(0);
		}
	} else {
		if($pid > 0) {
			exit(0);
		}
	}
} else {
	exit(1);
}

if(-f "cmd_q_shows-dag-B_A.log") {
} else {
	die "cmd_q_shows-dag-B_A.log does not exist\n";
}
my $logsize = -s "cmd_q_shows-dag-B_A.log";
#my @statl = stat 'cmd_q_shows-dag-B_A.log';
#my $size = $statl[7];
my $size = $logsize;

while(1) {
	sleep 10;
	$logsize = -s "cmd_q_shows-dag-B_A.log";
	$size = $logsize;
	print ".";
	my $oldsize = $size;
	$size = $statl[7];
	last if($size != $oldsize);
}
print "\n";

my @condorq = ();
runCondorTool("condor_q -dag",\@condorq, 2, {emit_output=>1});
my $endline = $#condorq - 2;
@condorq = @condorq[4..$endline];
open OUT,">cmd_q_shows-dag.output";
foreach (@condorq){
	print OUT $_;
}

print "Leaving x_condor_q_dag.pl\n";
