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

use strict;
use warnings;

foreach my $file (@ARGV)
{	
	my $filechanging = 0;
	#print "Currently scanning $file for CondorTest and prints\n";
	open(FILE,"<$file") || die "Can not open $file for reading: $!\n";
	open(NEW,">$file.tmp") || die "Can not open Temp file: $!\n";
	#print "$file opened OK!\n";
	my $line = "";
	while(<FILE>)	
	{		
		chomp();
		$line = $_;
		if($filechanging == 0) {
			if($line =~ /^\s*use\s*CondorPersonal.*$/) {
				# do nothing if going to a file
				$filechanging = 1;
				print "Rewriting $file\n";
				# Note we dropped the use CondorPersonal
			} else {
				# simply move content to new file
				print NEW "$line\n";
			}
		} else {
			# now we make selective changes to remove use of CondorPersonal
			if($line =~ /^(.*)CondorPersonal::SaveMeSetup.*$/) {
				print NEW "$1\$\$;\n";
			} elsif($line =~ /^(.*)CondorPersonal::StartCondor(.*)$/) {
				print NEW "$1CondorTest::StartPersonal$2\n";
			} elsif($line =~ /^(.*)CondorPersonal::KillDaemonPids(.*)$/) {
				print NEW "$1CondorTest::KillPersonal$2\n";
			} elsif($line =~ /^(.*)CondorPersonal::PersonalSystem(.*)$/) {
				print NEW "$1system$2\n";
			} else {
				print NEW "$line\n";
			}
		}

	}	
	close(FILE);
	close(NEW);
	if($filechanging == 1) {
		system("cp $file.tmp $file");
		system("chmod 755 $file");
		system("rm $file.tmp");
	} else {
		system("rm $file.tmp");
	}
}
exit(0);

