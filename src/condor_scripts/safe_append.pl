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

# safe_append.pl
#
# a simple perl script for use in the Condor test suite build system
# to append a line to a file if that file does not already include the
# given line.  We currently use this to generate the list of compiler
# directories and individual tests we care about on a given platform.
#
# Author: Derek Wright <wright@cs.wisc.edu> 2004-06-10


while( $_ = shift( @ARGV ) ) {
  SWITCH: {
      if( /-a.*/ ) {
	  $append = shift(@ARGV);
	  next SWITCH;
      }
      if( /-f.*/ ) {
	  $file = shift(@ARGV);
	  next SWITCH;
      }
      if( /-v.*/ ) {
	  $verbose = 1;
      }
  }
}

if( ! $file ) {
    die "You must specify a file to append to with '-f'";
}

if( ! $append ) {
    die "You must specify a text to append with '-a'";
}

open FILE, "<$file" || die "Can't open $file: $!\n";
@contents = <FILE>;
close FILE;
if( grep {$_ eq "$append\n"} @contents ) {
    # nothing to do;
    exit 0;
}

open FILE, ">>$file" || die "Can't open $file: $!\n";
print FILE "$append\n";
close FILE;
if( $verbose ) {
    print "safe_append.pl: added \"$append\" to $file\n";
}
exit 0;

