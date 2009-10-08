#!/usr/bin/perl

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


$SH_EXE = "C:\\cygwin\\bin\\sh.exe";
$PERL_EXE = "perl.exe";

%mappings = (
    "\\s*/bin/sh.*"                                => $SH_EXE,
    "\\s*(/usr/bin/env\\s+perl)|(/usr/bin/perl).*" => $PERL_EXE
);

sub check_wrapper
{
    if (!open(FILE, "<$ARGV[0]")) {
    	return;
    }

    # read in the first couple bytes and check for a shebang
    if ((read(FILE, $first_two, 2) != 2) or ($first_two ne "#!")) {
        close(FILE);
        return;
    }

    # file starts with shebang; get the rest of the line
    $line = <FILE>;
    
    # go through the mappings, looking for a match
    foreach $regex (keys %mappings) {
        if ($line =~ m{$regex}) {
            # found a match; prepend the path to the right interpreter
            unshift @ARGV, $mappings{$regex};
            break;
        }
    }
    close(FILE);
}

# prepend appropriate interpreter to @ARGV (if needed)
check_wrapper();

# TODO: this script currently only supports a limited class
#       of arguments. perl's system() function seems to
#       construct the command line by quoting any argument
#       with spaces in it (unless its already quoted ???)
#       there needs to be a function that will properly
#       quote all of @ARGV before calling system(). the main
#       thing that seems to break because of this is having
#       double quote characters in an argument

# execute the job
system {$ARGV[0]} @ARGV;

if ($? == -1) {
	die "failed to run \"$ARGV[0]\": $!";
}
elsif ($? & 127) {
	# TODO: should kill ourselves with the same signal
	# (for now just print a message)
	my $signo = $? & 127;
	die "\"$ARGV[0]\" exited with signal $signo\n";
}
else {
        # just exit with the same return code as the job
	exit($? >> 8);
}

