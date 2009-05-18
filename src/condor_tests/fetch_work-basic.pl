#! /usr/bin/env perl
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

$workdir = $ARGV[0]; 
$resultsdir = $ARGV[1];
$version = $ARGV[2];
$template = "x_workfetch_classad.1.template";
$classad = "x_workfetch_classad.1";
$line = "";
$counter = 0;
$maxwait = 1200; # better happen in 3 minutes

print scalar localtime() . "\n";

# create unique classad
open(TEMP,"<$template") || die "Can not open $template:$!\n";
open(CA,">$classad") || die "Can not open $classad:$!\n";
while(<TEMP>) {
	chomp();
	$line = $_;
	if($line =~ /^.*XXXXXX.*$/ ) {
		print CA "Iwd = \"\.\"\n";
	} elsif($line =~ /^.*YYYYYY.*$/ ) {
		print CA "Out = \"$resultsdir\/$version\"\n";
	} elsif($line =~ /^.*ZZZZZZ.*$/ ) {
		print CA "Args = \"$classad\"\n";
	} else {
		print CA "$line\n";
	}
}
close(TEMP);
close(CA);

# trigger the test by placing classad in work dir
print "Good  sending work\n";
#print "Good  sending copy to $resultsdir\n";
#print "Work is $classad\n";
system("cp $classad $resultsdir");
system("cp $classad $workdir");

# wait
print "Waiting - ";
while($counter < $maxwait) {
	if( ! ( -f "$resultsdir/$classad.results")) {
		print scalar localtime() . "\n";
		#print "Waiting for $resultsdir/$classad.results\n";
		system("ls $resultsdir");
		sleep 6;
	} else {
		last;
	}
	$counter += 3;
}
if( $counter >= $maxwait ) {
	print "bad\n";
	#print "Error took $maxwait seconds\n";
	exit(1);
} else {
	print "ok\n";
}

$status = system("diff $resultsdir/$version $resultsdir/$classad");
print "Status on diff is <$status>\n";
exit($status);
