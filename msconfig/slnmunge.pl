#!/usr/bin/env perl 


# Copyright (C) 2009 Red Hat, Inc.
# 
# Licensed under the Apache License, Version 2.0 (the "License"); you
# may not use this file except in compliance with the License.  You may
# obtain a copy of the License at
# 
#    http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# slnmunge.pl
#
# removes projects (specified on command line) from a
# VS "solution" file piped in from stdin, writes munged
# solution file to stdout
# 
#   example:  
#     slnmunge.pl condor_birdwatcher < condor.sln > condor2.sln
#
# Mea culpa for any unidiomatic perl.
#
# Will Benton, 12/17/2008

$skip = 0;
@guids = ();
@output = ();
$skipped_projects = join("|", @ARGV);

if (scalar(@ARGV) == 0) {
    while (<STDIN>) {
	print $_;
    }
    exit(0);
}

IN: while (<STDIN>) {
    if ($skip) {
	$skip = 0;
	next IN;
    }

    if (/Project\("\{([0-9A-F-]*)\}"\) = "($skipped_projects)", "(\2.vcproj)", "\{([0-9A-F-]*)\}".*$/) {
	push @guids, $4;
	$skip = 1;
	next IN;
    }

    push @output, $_;
}

OUT: foreach (@output) {
    my $line = $_;
    foreach (@guids) {
	my $guid = $_;
	if ($line =~ /$guid/) {
	    next OUT;
	}
    }
    print $line;
}
