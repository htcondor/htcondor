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

use strict;
use warnings;

if ($#ARGV < 0) {
	# Echo stdin to stdout.
	while (<>){
		print
	}
} else {
	# Open and print files from args.
	my $file;
	my $ifh;
	while ($file = shift @ARGV) {
		# if $file starts with $, assume it is an env var, and expand
		if ($file =~ s/^\$//) {
			$file = $ENV{"$file"};
		}
		open $ifh, "<", $file or die $!;
		while (<$ifh>){
			print
		}
		close $ifh;
	}
}

exit(0);
