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


my $dupclass = $ARGV[0];
my $newclass = $ARGV[1];
my $dupfile = $ARGV[2];
my $newfile = $dupfile . ".new";


#my $dbtimelog = "x_strip.log";
#print "Trying to open logfile... $dbtimelog\n";
#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">>$dbtimelog") or die "Could not open $dbtimelog: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR);
 #$| = 1;
#select(STDOUT);
 ##$| = 1;


open(OLD,"<$dupfile") || die "Can not open $dupfile to dupe:$!\n";
open(NEW,">$newfile") || die "Can not open new file $newfile:$!\n";
$line = "";
while(<OLD>) {
	chomp;
	$line = $_;
	if( $line =~ /^TESTCLASS\((.*),$dupclass\).*$/ ) {
		print "Found $dupclass test <$1>\n";
		print NEW "$line\n";
		print NEW "TESTCLASS($1,$newclass)\n"
	} else {
		print NEW "$line\n";
	}
		
}
close(OLD);
close(NEW);
