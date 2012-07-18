#!/usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2012, Condor Team, Computer Sciences Department,
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

use File::Copy;
use File::Path;
use Getopt::Long;
use Cwd;

print "Called with:" . join(" ", @ARGV) . "\n";

my ($help, $noinput, $noextrainput, $extrainput, $onesetout, $long, $forever, $job, $failok);
GetOptions (
    'help'         => \$help,
    'noinput'      => \$noinput,
    'noextrainput' => \$noextrainput,
    'extrainput'   => \$extrainput,
    'onesetout'    => \$onesetout,
    'long'         => \$long,
    'forever'      => \$forever,
    'job=i'        => \$job,
    'failok'       => \$failok,
);

if($help) {
    help();
    exit(0);
}

my $workingdir = getcwd();
print "Working Directory is $workingdir\n";

# test basic running of script
if ( $noinput ) {
    print "Trivial success\n";
    print "Case noinput\n";
    exit(0)
}

my $Idir = "job_" . $job . "_dir";

# test for input = filenm
if ( $noextrainput ) {
    print "Case noextrainput\n";
    # test and leave
    my $Ifile = "submit_filetrans_input" . $job . ".txt";
    print "Looking for input file $Ifile\n";
    system("ls submit_filetrans_input*");
    if( -f "$Ifile" ) {
        print "Input file arrived\n";
        # reverse logic applies failing is good and working is bad. exit(0);
        if($failok) {
            exit(1);
        }
        else {
            exit(0);
        }
    }
    else {
        print "Input file failed to arrive\n";
        if($failok) {
            exit(0);
        }
        else {
            exit(1);
        }
    }
}


if( $extrainput ) {
    print "Case extrainput\n";
    # test and leave
    my $Ifile1 = "submit_filetrans_input"."$job"."a.txt";
    my $Ifile2 = "submit_filetrans_input"."$job"."b.txt";
    my $Ifile3 = "submit_filetrans_input"."$job"."c.txt";
    if(!-f $Ifile1) { 
        print "$Ifile1 failed to arrive\n"; 
        if($failok) {
            exit(0);
        }
        else {
            exit(1);
        }
    }
    if(!-f $Ifile2) { 
        print "$Ifile2 failed to arrive\n"; 
        if($failok) {
            exit(0);
        }
        else {
            exit(1);
        }
    }
    if(!-f $Ifile3) { 
        print "$Ifile3 failed to arrive\n"; 
        if($failok) {
            exit(0);
        }
        else {
            exit(1);
        }
    }
    # test done leave
    print "Extra input files arrived\n";
    # reverse logic applies failing is good and working is bad. exit(0);
    if($failok) {
        exit(1);
    }
    else {
        exit(0);
    }
}

if(!defined($job)) {
    print "No processid given for output file naming\n";
    exit(1);
}

print "PID = $job  (will be used as identifier for files)\n";
system("mkdir -p dir_$job");
chdir("dir_$job");
my $out = "submit_filetrans_output";
my $out1 = $out . $job . "e.txt";
my $out2 = $out . $job . "f.txt";
my $out3 = $out . $job . "g.txt";
my $out4 = $out . $job . "h.txt";
my $out5 = $out . $job . "i.txt";
my $out6 = $out . $job . "j.txt";

system("touch $out1 $out2 $out3");
print "We should see 3 files now:\n";
system("ls");
system("cp * ..");

if($onesetout) {
    # create and leave
    print "Case onesetout\n";
    exit(0);
}

#allow time for vacate
sleep 20;

system("touch $out4 $out5 $out6");
print "We should see 3 more files now:\n";
system("ls");
system("cp * ..");
print "Sandbox now contains:\n";
system("ls ..");

if( $long ) { 
    # create and leave
    print "Case long\n";
    exit(0);
}

if( $forever ) {
    while(1) {
        sleep(1);
    }
}

# =================================
# print help
# =================================

sub help {
print "Usage: $0
Options:
[-h/--help]         See this help message
[--noinput]         Only the script is needed, no checking
[--noextrainput]    Only the script is needed and submit_filetrans_input.txt
[--onesetout]       Create one set of output files and then exit
\n";
}
