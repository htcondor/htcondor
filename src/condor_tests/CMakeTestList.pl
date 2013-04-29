#! /usr/bin/env perl
use Getopt::Long;
use Cwd;


# Dagman passes node which maps to both where the job is running
$cwd = getcwd();

#my $dbinstalllog =  "MkTestList.log";
#print "Trying to open logfile... $dbinstalllog\n";
#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">>$dbinstalllog") or die "Could not open $dbinstalllog: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR);
 #$| = 1;
#select(STDOUT);
 #$| = 1;
#
my $testlistname =  "Cmake_batch_tests_list.txt";
my $source = "CMakeLists.txt";
my $line = "";

open(IN,"<$source") or die "Can not open $source:$!\n";
open(OUT,">$testlistname") or die "Can not open $testlistname:$!\n";
while(<IN>) {
	chomp();
	$line = $_;
	if($line =~ /^\s*condor_pl_test\s*\(\s*(.*?)\s.*$/) {
		print OUT "$1\n";
	}
}

exit(0);

