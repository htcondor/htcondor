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


######################################################################
# $Id: remote_post,v 1.4 2007-11-08 22:53:44 nleroy Exp $
# post script to cleanup after a Condor build, successful or not
######################################################################

# autoflush our STDOUT
$| = 1;

my $boos = 1; # build out of source

use strict;
use warnings;
use Cwd;
use File::Basename;
use File::Copy;

my $BaseDir = getcwd();
my $BldDir = "$BaseDir/src";  # directory for build products
my $SrcDir = $BldDir;         # directory of sources (git clone/unpacked source tarball)
if ($boos) { $SrcDir =~ s/userdir/sources/; }
my $pub_dir = "$BaseDir/public";
#my $pub_logs = "$pub_dir/logs";
#my $pub_testbin = "$pub_dir/testbin";

if( defined $ENV{_NMI_STEP_FAILED} ) { 
    my $exit_status = 1;
    my $fd;
    if( open( $fd, "<", ".nmi_failed_task_names" ) ) {
        $exit_status = 0; # assume that only unit tests failed.
        while( my $line = <$fd> ) {
            print( "A task failed: $line" );
            if( ! ($line =~ /run_unit_tests/) ) {
                $exit_status = 1; # we have failures other than unit tests
            }
        }
        close( $fd );
    } else {
        print "A previous step failed but no .nmi_failed_tasks file was found.\n  _NMI_STEP_FAILED is: '$ENV{_NMI_STEP_FAILED}'\n";
    }
    if ($exit_status) {
        print "Exiting now with exit status $exit_status.\n";
        exit $exit_status;
    }
}
else {
    print "The _NMI_STEP_FAILED variable is not set\n";
}

if ($ENV{NMI_PLATFORM} =~ /_win/i) {
    # leave the path alone in new-batlab.  lets see how that works out.
    print "In new batlab, leaving path alone\n";
} elsif ($ENV{NMI_PLATFORM} =~ /macos/i) {
	# CRUFT Once 9.0 is EOL, remove the python.org version of python3
	#   from the mac build machines and remove this setting of PATH.
	# Ensure we're using the system python3
	$ENV{PATH} ="/usr/bin:$ENV{PATH}";
}
print "------------------------- ENV DUMP ------------------------\n";
foreach my $key ( sort {uc($a) cmp uc($b)} (keys %ENV) ) {
    print "$key=".$ENV{$key}."\n";
}
print "------------------------- ENV DUMP ------------------------\n";


######################################################################
# save debugging results
######################################################################

# create a directory to save the data
if( ! -d $pub_dir ) {
    mkdir( "$pub_dir", 0777 ) || die("Can't mkdir($pub_dir): $!\n");
}

# copy in the package targets
if( $ENV{NMI_PLATFORM} =~ /_win/i ) {
		my $bsdtar = "$BaseDir/msconfig/tar.exe";
		if ( -f "$bsdtar" ) {
			my $zips = ""; if ( <$BaseDir/*.zip> ) { $zips = '*.zip'; }
			my $tars = ""; if ( <$BaseDir/*.gz> ) { $tars = '*.gz'; }
			my $msis = ""; if ( <$BaseDir/*.msi> ) { $msis = '*.msi'; }
			my $reldir = ""; if ( -d "$BaseDir/release_dir" ) { $reldir = "release_dir"; }
			print "Is param_info_tables.h here: $BldDir/condor_utils?\n";
			if ( -f "$BldDir/condor_utils/param_info_tables.h") {
				print "copying $BldDir/condor_utils/param_info_tables.h, $BldDir/condor_tests/param_info_tables.h\n";
				copy("$BldDir/condor_utils/param_info_tables.h", "$BldDir/condor_tests/param_info_tables.h");
				print "copying $SrcDir/condor_utils/param_info.in, $BldDir/condor_tests/param_info.in\n";
				copy("$SrcDir/condor_utils/param_info.in", "$BldDir/condor_tests/param_info.in");
			} else {
				print "looks like no...\n";
				system("dir $BldDir/condor_utils");
			}
			print "Is msconfig here: $BaseDir?\n";
			if ( -d "$BaseDir/msconfig") {
			} else {
				print "looks like no...\n";
				system("dir $BaseDir");
			}
			if (1) {
				if ($boos) {
					#system("xcopy /s/e $SrcDir\\condor_tests\\* $BldDir\\condor_tests");
					#system("xcopy /s/e $SrcDir\\condor_examples\\* $BldDir\\condor_examples");
				}
			} else {
				$_ = $BaseDir;
				s/\//\\\\/g;
				my $winbasedir = $_;
				$winbasedir = $winbasedir . "\\msconfig";
				$_ = $BldDir;
				s/\//\\\\/g;
				my $winsrcdir = $_;
				my $wintestloc = $winsrcdir . "\\condor_tests";
				my $winutilloc = $winsrcdir . "\\condor_utils\\param_info_tables.h";
				my $winutilloc2 = $winsrcdir . "\\condor_utils\\param_info.in";
			
				my $xcopy1 = "xcopy $winbasedir $wintestloc /E /y";
				print "Get msconfig into test folder:$xcopy1\n";
				system("$xcopy1");
				my $xcopy2 = "xcopy  $winutilloc $wintestloc /E /y";
				print "get param_info_tables.h into test folder:$xcopy2\n";
				system("$xcopy2");
				my $xcopy3 = "xcopy  $winutilloc2 $wintestloc /E /y";
				print "get param_info.in into test folder:$xcopy3\n";
				system("$xcopy3");
				print "$BldDir now:\n";
				system("dir $BldDir");
			}
			print "Tarring up results and tests ($zips $tars $msis $reldir)\n";
			open( TAR, "$bsdtar -czvf results.tar.gz -C $BaseDir $zips $tars $msis $reldir msconfig -C $BldDir condor_tests condor_examples |" ) || 
				die "Can't open tar as a pipe: $!\n";
			while( <TAR> ) { 
				print;
			}
			close( TAR );
			my $tarstatus = $? >> 8;
			if( $tarstatus ) {
				die "Can't tar zcf results.tar.gz public: status $tarstatus\n";
			}
			print "Done bsd-tarring results\n";
			exit 0;
		} else {
			print "ERROR: $BaseDir/msconfig/tar.exe does not exist!\n";
			exit 1;
		}
}
else {
    system("mv *.tar.gz $pub_dir");
    if (glob("*.rpm")) {
        system("mv *.rpm $pub_dir");
    } elsif (glob("*.changes")) {
        system("mv *.changes *.dsc *deb *.debian.tar.xz $pub_dir");
    }
    system("cmake -E md5sum $pub_dir/* md5s.txt");
}


# copy all the bits in which are required to run batch_test (omg what a cf) 
if( $ENV{NMI_PLATFORM} =~ /AlmaLinux|AmazonLinux|CentOS|Fedora|openSUSE|Rocky|Debian|Ubuntu/i ) {
    system("tar xfp $pub_dir/condor_tests-*.tar.gz");
    system("mv condor_tests-*/condor_tests $pub_dir");
    system("rmdir condor_tests-*");
} else {
    system("cp -r $BldDir/condor_tests $pub_dir");
    system("cp  $BldDir/condor_utils/param_info_tables.h $pub_dir/condor_tests");
}
system("cp -r $BldDir/condor_examples $pub_dir");
# copy src/condor_utils/param_info_tables.h to condor_tests for parsing in param completeness test
# ticket #3877 for param system completeness test.
# We take the latest of this file, parse with a c program which
# drops the current into an easy format for testing for completeness
# in our framework.
system("cp  $SrcDir/condor_utils/param_info.in $pub_dir/condor_tests");

######################################################################
# tar up build results
######################################################################

print "Tarring up results\n";
open( TAR, "tar zcvf results.tar.gz public|" ) || 
    die "Can't open tar as a pipe: $!\n";
while( <TAR> ) { 
    print;
}
close( TAR );
my $tarstatus = $? >> 8;
if( $tarstatus ) {
    die "Can't tar zcf results.tar.gz public: status $tarstatus\n";
}
print "Done tarring results\n";
exit 0;
