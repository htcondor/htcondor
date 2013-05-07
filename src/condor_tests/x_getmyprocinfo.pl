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


my $procinfo = $ARGV[0];
my $searchfor = $ARGV[1];

my $pid = $$;
#print "My PID = $pid\n";

#my $dbtimelog = "x_change.log";
#print "Trying to open logfile... $dbtimelog\n";
#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">>$dbtimelog") or die "Could not open $dbtimelog: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR);
 #$| = 1;
#select(STDOUT);
 #$| = 1;

my $line = "";
if(!("/proc")) {
	die "Not a system with \/proc\n";
} else {
	if(defined $searchfor) {
		open(SP,"</proc/$pid/$procinfo") or die "Failed to open /proc/$pid/$procinfo:$!\n";
		while(<SP>) {
			chomp();
			$line = $_;
			if($line =~ /$searchfor/) {
				print "$line\n";
			}
		}
		close(SP);
	} else {
		system("cat \/proc/$pid\/$procinfo");
	}
}
