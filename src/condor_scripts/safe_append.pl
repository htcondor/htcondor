#!/usr/bin/env perl

############################Copyright-DO-NOT-REMOVE-THIS-LINE##
#
# Condor Software Copyright Notice
# Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# This source code is covered by the Condor Public License, which can
# be found in the accompanying LICENSE.TXT file, or online at
# www.condorproject.org.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
# FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
# HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
# MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
# ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
# PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
# RIGHT.
#
############################Copyright-DO-NOT-REMOVE-THIS-LINE##


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
    print "Added $append to $file\n";
}
exit 0;

