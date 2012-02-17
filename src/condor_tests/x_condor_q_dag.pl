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

my $pid = fork;
if (defined $pid ) {
	if($pid > 0) {
		exit(0);
	}
} else {
	exit(1);
}

my @statl = stat 'cmd_q_shows-dag-B_A.log';
my $size = $statl[7];

while(1) {
	sleep 10;
	@statl = stat 'cmd_q_shows-dag-B_A.log';
	my $oldsize = $size;
	$size = $statl[7];
	last if($size != $oldsize);
}

open CONDORQ,"condor_q -dag|" || die "Could not run condor_q";
my @condorq = <CONDORQ>;
my $endline = $#condorq - 2;
@condorq = @condorq[4..$endline];
open OUT,">cmd_q_shows-dag.output";
foreach (@condorq){
	print OUT $_;
}
