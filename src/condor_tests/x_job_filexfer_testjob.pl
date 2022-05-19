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
my $btdebug = 1;

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
CreateDir("-p dir_$job");
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
CopyIt("$out1 ..");
CopyIt("$out2 ..");
CopyIt("$out3 ..");

if($onesetout) {
    # create and leave
    print "Case onesetout\n";
	system("ls -la");
    exit(0);
}

#allow time for vacate
sleep 20;

CreateNonEmptyFile("$out4");
CreateNonEmptyFile("$out5");
CreateNonEmptyFile("$out6");
CopyIt("$out4 ..");
CopyIt("$out5 ..");
CopyIt("$out6 ..");

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

sub CreateDir
{
	my $cmdline = shift;
	my @argsin = split /\s/, $cmdline;
	my $cmdcount = @argsin;
	my $ret = 0;
	my $fullcmd = "";
	my $location = Cwd::getcwd();
	#print  "\n\n\n\n\n******* CreateDir: $cmdline argcout:$cmdcount while here:$location *******\n\n\n\n\n";

	my $amwindows = is_windows();

	my $winpath = "";
	if($amwindows == 1) {
		if(is_windows_native_perl()) {
			#print "CreateDir:windows_native_perl\n";
			# what if a linux path first?
			if($argsin[0] eq "-p") {
				shift @argsin;
			}
			foreach my $dir (@argsin) {
				#print "Want to make:$dir\n";
				$_ = $dir;
				s/\//\\/g;
				s/\\/\\\\/g;
				$dir = $_;
				if(-d "$dir") {
					next;
				}
				#print "$dir does not exist yet\n";
				$fullcmd = "cmd /C mkdir $dir";
				$ret = system("$fullcmd");
				if($ret != 0) {
					print "THIS:$fullcmd Failed\n";
				} else {
						#print "THIS:$fullcmd worked\n";
						#print "If this worked, it should exist now.\n";
						#if(-d $dir) {
							#print "Perl says it does.\n";
						#} else {
							#print "Perl says it does NOT.\n";
						#}
				}
			}
			#print "CreateDir returning now: Return value from CreateDir:$ret\n";
			return($ret);
		} else {
			if($argsin[0] eq "-p") {
				$winpath = `cygpath -w $argsin[1]`;
				CondorUtils::fullchomp($winpath);
				$_ = $winpath;
				s/\\/\\\\/g;
				$winpath = $_;
				if(-d "$argsin[1]") {
					return($ret);
				}
			} else {
				$winpath = `cygpath -w $argsin[0]`;
				CondorUtils::fullchomp($winpath);
				$_ = $winpath;
				s/\\/\\\\/g;
				$winpath = $_;
				if(-d "$argsin[0]") {
					return($ret);
				}
			}
		}

		$fullcmd = "cmd /C mkdir $winpath";
		$ret = system("$fullcmd");
	} else {
		$fullcmd = "mkdir $cmdline"; 	
		if(-d $cmdline) {
			return($ret);
		}
		$ret = system("$fullcmd");
		#print "Tried to create dir got ret value:$ret path:$cmdline/$fullcmd\n";
	}
	return($ret);
}


sub is_windows {
    if (($^O =~ /MSWin32/) || ($^O =~ /cygwin/)) {
        return 1;
    }
    return 0;
}

sub is_cygwin_perl {
    if ($^O =~ /cygwin/) {
        return 1;
    }
    return 0;
}

sub is_windows_native_perl {
    if ($^O =~ /MSWin32/) {
         return 1;
    }
    return 0;
}

sub CopyIt
{
    my $cmdline = shift;
    my $ret = 0;
    my $fullcmd = "";
    my $dashr = "no";
	#print  "CopyIt: $cmdline\n";
    my $winsrc = "";
    my $windest = "";

    my $amwindows = is_windows();
    if($cmdline =~ /\-r/) {
        $dashr = "yes";
        $_ = $cmdline;
        s/\-r//;
        $cmdline = $_;
    }
    # this should leave us source and destination
    my @argsin = split /\s/, $cmdline;
    my $cmdcount = @argsin;

	if($btdebug == 1) {
		print "CopyIt command line passed in:$cmdline\n";
	}
    if($amwindows == 1) {
		if(is_windows_native_perl()) {
			#print "CopyIt: windows_native_perl\n";
			$winsrc = $argsin[0];
			$windest = $argsin[1];
			#print "native perl:\n";
			# check target
			$windest =~ s/\//\\/g;
        	$fullcmd = "xcopy $winsrc $windest /Y";
			#print "native perl:$fullcmd\n";
        	if($dashr eq "yes") {
            	$fullcmd = $fullcmd . " /s /e";
				#print "native perl -r:$fullcmd\n";
        	}
		} else {
        	$winsrc = `cygpath -w $argsin[0]`;
        	$windest = `cygpath -w $argsin[1]`;
        	CondorUtils::fullchomp($winsrc);
        	CondorUtils::fullchomp($windest);
        	$_ = $winsrc;
        	s/\\/\\\\/g;
        	$winsrc = $_;
        	$_ = $windest;
        	s/\\/\\\\/g;
        	$windest = $_;
        	$fullcmd = "xcopy $winsrc $windest /Y";
        	if($dashr eq "yes") {
            	$fullcmd = $fullcmd . " /s /e";
        	}

		}
        $ret = system("$fullcmd");
		if($btdebug == 1) {
			print "Tried to create dir got ret value:$ret cmd:$fullcmd\n";
		}
    } else {
        $fullcmd = "cp ";
        if($dashr eq "yes") {
            $fullcmd = $fullcmd . "-r ";
        }
        $fullcmd = $fullcmd . "$cmdline";
		print "About to  execute this<$fullcmd>\n";
        $ret = system("$fullcmd");

		#print "Tried to create dir got ret value:$ret path:$cmdline\n";
    }
	if($btdebug == 1) {
		print "CopyIt returning:$ret\n";
	}
	
    return($ret);
}

