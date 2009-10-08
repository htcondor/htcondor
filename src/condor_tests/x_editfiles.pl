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

my $newfile = "";
my $line = "";
foreach my $file (@ARGV) {
	$newfile = "$file.new";
	open(NF,">$newfile") or die "Can not create $newfile:$!\n";
	open(OF,"<$file") or die "Can not open $file:$!\n";
	while(<OF>) {
		chomp();
		$line = $_;
		if($line =~ /^executable\s+=\s+(.*)\.exe\..*/) {
			print NF "executable = $1.exe.LINUX.INTEL\n";
			print NF "requirements = (Arch == \"X86_64\")\n";
		} else {
			print NF "$line\n";
		}

	}
	system("mv $newfile $file");
	print "$file\n";
}
exit(0);

