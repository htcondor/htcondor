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
		'data=s' => \$datafile,
		'out=s' => \$outfile,
);

if ( $help )    { help() and exit(0); }
if( !$datafile ) {
	die "Have to have an input file!\n";
}

if( $datafile ) {
	print "Merging daily data in <<$datafile>>\n";
}

if( !$outfile ) {
	die "Must have a out file\n";
}

my $curday = "";
my $lastday = "";
my $curyear = "";
my $curmonth = "";
my $lastmonth = "";
my $line = "";
my $daysinmonth = 0;
my $pendtotal = 0;
my $failtotal = 0;
my $pfailtotal = 0;
my $goodtotal = 0;
my $pfgoodtotal = 0;

open(OUT,">$outfile") || die "Failed to open output: $!\n";
open(IN,"<$datafile") || die "Failed to open input: $!\n";
while(<IN>) {
	@aday = split /,/, $_;
	($curday, $curmonth, $curyear) = split / /, $aday[0];
	#print "$_";
	if($lastmonth ne $curmonth) {
		#Reset and compute last month
		#print "Reset and compute\n";
		if($daysinmonth != 0 ) {
			ComputeReset();
		}
		$lastmonth = $curmonth;
		$lastday = $curday;
		$daysinmonth = $daysinmonth + 1; # first day of month
	} else {
		if($curday != $lastday ) {
			$daysinmonth = $daysinmonth + 1;
			$lastday = $curday;
		}
		$pendtotal = $pendtotal + $aday[1];
		$failtotal = $failtotal + $aday[2];
		$pfailtotal = $pfailtotal + $aday[3];
		$goodtotal = $goodtotal + $aday[4];
		$pfgoodtotal = $pfgoodtotal + $aday[5];
	}
	#print "$aday[0]\n";
	#print "$curmonth\n";
}

sub ComputeReset
{
	#compute portion and output
	#print "(p) $pendtotal (f) $failtotal (pf) $pfailtotal (g) $goodtotal (pfg) $pfgoodtotal (days) $daysinmonth\n";
	$pendtotal = $pendtotal / $daysinmonth;
	$failtotal = $failtotal / $daysinmonth;
	$pfailtotal = $pfailtotal / $daysinmonth;
	$goodtotal = $goodtotal / $daysinmonth;
	$pfgoodtotal = $pfgoodtotal / $daysinmonth;
	#print "(p) $pendtotal (f) $failtotal (pf) $pfailtotal (g) $goodtotal (pfg) $pfgoodtotal (days) $daysinmonth\n";
	print OUT "28 $curmonth $curyear, $pendtotal, $failtotal, $pfailtotal, $goodtotal, $pfgoodtotal\n";

	#reset portion
	$daysinmonth = 0;
	$pendtotal = 0;
	$failtotal = 0;
	$pfailtotal = 0;
	$goodtotal = 0;
	$pfgoodtotal = 0;
}
