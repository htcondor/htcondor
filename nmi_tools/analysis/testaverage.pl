#!/usr/bin/perl
use File::Copy;
use File::Path;
use Getopt::Long;

my %months = (
	"1" => "January",
	"2" => "February",
	"3" => "March",
	"4" => "April",
	"5" => "May",
	"6" => "June",
	"7" => "July",
	"8" => "August",
	"9" => "September",
	"10" => "October",
	"11" => "November",
	"12" => "December",
);

GetOptions (
		'help' => \$help,
		'builds=s' => \$buildsin,
		'tests=s' => \$testsin,
		'out=s' => \$outfile,
);

my %testdates;
my %builddates;
my @tests;
my @builds;

if ( $help )    { help() and exit(0); }

if( !$buildsin ) {
	die "Have to have an build input file!\n";
}

if( !$testsin ) {
	die "Have to have an tests input file!\n";
}

if( !$outfile ) {
	die "Must have a out file\n";
}

open(BUILDS,"<$buildsin") || die "Can not open build input file($buildsin):$!\n";
open(TESTS,"<$testsin") || die "Can not open build input file($testsin):$!\n";
open(OUT,">$outfile") || die "Can not open build input file($outfile):$!\n";

# stash builds

my $line = "";
while(<BUILDS>) {
	chomp;
	$line = $_;
	@abuildavg = split /,/, $line;
	$builddates{"$abuildavg[0]"} = $abuildavg[5];
	push(@builds, $line);
}

# stash tests

my $line = "";
while(<TESTS>) {
	chomp;
	$line = $_;
	@atestavg = split /,/, $line;
	$testdates{"$atestavg[0]"} = $atestavg[5];
	push(@tests, $line);
}

foreach $avg (@builds) {
	#print "$avg\n";
	@tmp = split /,/, $avg;
	$testsper;
	# Matching build and test data?
	if((exists $builddates{"$tmp[0]"}) && (exists $testdates{"$tmp[0]"})) {
		$testsper =  $testdates{"$tmp[0]"}/$builddates{"$tmp[0]"}; 
		print OUT "$tmp[0], $testsper\n";
	}
}

# go through innorder one array and if tests
# and builds exist for that day, drop data
close(BUILDS);
close(TESTS);
close(OUT);
