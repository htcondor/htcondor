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

my ($help, $noinput, $noextrainput, $extrainput, $onesetout, $long, $forever, $job, $failok, $stderr_str);
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
    'stderr=s'     => \$stderr_str,
);

if($help) {
    help();
    exit(0);
}

if ( $stderr_str ) {
	# Print the reqeusted line to stderr, flushing stdout so the line
	# appears interleaved with stdout when the two are mergd.
	select(STDOUT);
	$| = 1;
    print STDERR "$stderr_str\n";
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
    print "$_\n" foreach glob("submit_filetrans_input*");
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
CreateDir("dir_$job");
chdir("dir_$job");
my $out = "submit_filetrans_output";
my $out1 = $out . $job . "e.txt";
my $out2 = $out . $job . "f.txt";
my $out3 = $out . $job . "g.txt";
my $out4 = $out . $job . "h.txt";
my $out5 = $out . $job . "i.txt";
my $out6 = $out . $job . "j.txt";

CreateNonEmptyFile("$out1");
CreateNonEmptyFile("$out2");
CreateNonEmptyFile("$out3");
CopyIt($out1, "..");
CopyIt($out2, "..");
CopyIt($out3, "..");

if($onesetout) {
    # create and leave
    print "Case onesetout\n";
	ListDir(".");
    exit(0);
}

#allow time for vacate
sleep 20;

CreateNonEmptyFile("$out4");
CreateNonEmptyFile("$out5");
CreateNonEmptyFile("$out6");
CopyIt($out4, "..");
CopyIt($out5, "..");
CopyIt($out6, "..");

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

sub CreateEmptyFile {
    my $name = shift;
    open(NF,">$name") or die "Failed to create:$name:$!\n";
    print NF "";
    close(NF);
}

sub CreateNonEmptyFile {
    my $name = shift;
    open(NF,">$name") or die "Failed to create:$name:$!\n";
    print NF "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n";
    close(NF);
}

# Everything below is done inside this perl process rather than by running
# cmd/xcopy/ls.  On Windows a child process sharing the job's console can
# keep condor_softkill from finding the job's window during a vacate.

sub CreateDir
{
	my $dir = shift;
	if(-d $dir) {
		return(0);
	}
	File::Path::make_path($dir, { error => \my $err });
	if(@$err) {
		print "CreateDir: failed to create $dir\n";
		return(1);
	}
	return(0);
}

sub CopyIt
{
	my ($src, $dest) = @_;
	my $ret = 0;
	if(!copy($src, $dest)) {
		print "CopyIt: copy $src to $dest failed: $!\n";
		$ret = 1;
	}
	return($ret);
}

sub ListDir
{
	my $dir = shift;
	opendir(my $dh, $dir) or return;
	foreach my $entry (sort readdir($dh)) {
		my $size = -s "$dir/$entry";
		printf("%10s %s\n", defined($size) ? $size : "", $entry);
	}
	closedir($dh);
}
