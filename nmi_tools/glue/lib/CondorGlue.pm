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

######################################################################
# Perl module to handle a lot of shared functionality for the Condor
# build and test "glue" scripts for use with the NMI-NWO framework.
#
# Originally written by Derek Wright <wright@cs.wisc.edu> 2004-12-30
# $Id: CondorGlue.pm,v 1.3 2007-11-08 22:53:45 nleroy Exp $
#
######################################################################

package CondorGlue;

use POSIX;

sub parseTag
{
    my $tag = shift;

    my $desc;
    my @vers;

    $tag =~ s/BUILD-//;
    $desc = $tag;
    $tag =~ s/-branch-.*//;
    $tag =~ s/V//;
    if( $tag =~ /(\d+)(\D*)_(\d+)(\D*)_?(\d+)?(\D*)/ ) {
        $vers[0] = $1;
        $vers[1] = $3;
        if( $5 ) {
            $vers[2] = $5;
        } else {
            $vers[2] = "x";
        }
    }
    return ( $desc, @vers );
}


sub run
{
    my ($cmd, $fatal) = @_;
    my $ret;

    # if not specified, the command is fatal
    if( !defined($fatal) ) {
        $fatal = 1;
    }

    print "RUNNING: $cmd\n";

    # run the command
    system("($cmd)  </dev/null 2>&1");

    $ret = $?;
    # should we die here?
    if( $ret != 0 ) {
	print "RESULT: " . statusString($ret) . "\n";
	if( defined($fatal) && $fatal != 0 ) {
	    exit( $fatal );
	}
    }

    # return the command's return value
    return $ret;
}


sub statusString
{
    my $status = shift;

    if( $status == -1 ) {
        return "failed to execute: $!";
    } elsif( WIFEXITED($status) ) {
        return "exited with status: " . WEXITSTATUS($status);
    } elsif( WIFSIGNALED($status) ) {
        return "died with signal: " . WTERMSIG($status);
    } else {
        return "returned unrecognized status: $status";
    }
}


1;

