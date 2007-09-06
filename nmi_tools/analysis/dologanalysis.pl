#!/usr/bin/perl
use File::Copy;
use File::Path;
use Getopt::Long;

my $datalocation = "/p/condor/public/license/";
my $datacruncher = "filter_sendfile";
my $crunch = $datalocation . $datacruncher;
#my @datafiles = ( "sendfile-v6.0", "sendfile-v6.1", "sendfile-v6.2", "sendfile-v6.3", "sendfile-v6.4", "sendfile-v6.5", "sendfile-v6.6", "sendfile-v6.7", "sendfile-v6.8", "sendfile-v6.9" );
my @datafiles = ( "sendfile-v6.8" );

GetOptions (
		'help' => \$help,
		'plot' => \$plot,
);

my %months = (
    "Jan" => "January",
    "Feb" => "February",
    "Mar" => "March",
    "Apr" => "April",
    "May" => "May",
    "Jun" => "June",
    "Jul" => "July",
    "Aug" => "August",
    "Sep" => "September",
    "Oct" => "October",
    "Nov" => "November",
    "Dec" => "December",
);

my %nummonths = (
    "Jan" => 1,
    "Feb" => 2,
    "Mar" => 3,
    "Apr" => 4,
    "May" => 5,
    "Jun" => 6,
    "Jul" => 7,
    "Aug" => 8,
    "Sep" => 9,
    "Oct" => 10,
    "Nov" => 11,
    "Dec" => 12,
);

if ( $help )    { help() and exit(0); }

$line;
$totalgood = 0;
$totalbad = 0;
$negbad = 0;
$total = 0;
$scale = 0;
$month;
$day = 0;
$year;
$skip = "true";
$trimdata = "no";

print "$crunch\n";
foreach $log (@datafiles) {
	$datafile = $datalocation . $log;
	print "$log\n";
	$outfile = $log . ".data";
	$image = $log . ".png";
	$trimdata = "no";
	$skip = "true";
	open(LOGDATA,"$crunch $datafile 2>&1|") || die "logdata bad: $!\n";
	open(OUT,">$outfile") || die "Failed to open $outfile: $!\n";
	while(<LOGDATA>) {
		chomp;
		$line = $_;
		print "$_\n";
		if($line =~ /^\s+[\?MB]+\s+(\w+)\s+(\w+)\s+(\d+)\s+(\d+:\d+:\d+)\s+(\d+):.*/) {
			if( $skip eq "true" )  {
				$skip = "false";
				$firstdate = $3 . " " . $months{$2} . " " . $5;
				$month = $2;
				print "Starting: $firstdate\n";
			} else {
				if( $day ne $3){ 
					$scale = ScaleData();
					DoOutput($scale);
					ResetCounters();
				}
				$trimdata = CheckForEnd( $month, $2, $3, $5 );
			}
			$day = $3;
			$month = $2;
			$year = $5;
			$totalgood = $totalgood + 1;
			print "Good $2 $3 $5\n";
		} elsif($line =~ /^\*[\?MB]+\s+(\w+)\s+(\w+)\s+(\d+)\s+(\d+:\d+:\d+)\s+(\d+):.*/) {
			if( $skip eq "true" )  {
				$skip = "false";
				$firstdate = $3 . " " . $months{$2} . " " . $5;
				$month = $2;
				print "Starting: $firstdate\n";
			} else {
				if( $day ne $3){ 
					$scale = ScaleData();
					DoOutput($scale);
					ResetCounters();
				}
				$trimdata = CheckForEnd( $month, $2, $3, $5 );
			}
			$day = $3;
			$month = $2;
			$year = $5;
			$totalbad = $totalbad + 1;
			print "Bad $2 $3 $5\n";
		} else {
			print "Can not determine\n";
		}

		if($trimdata eq "yes") {
			last;
		}
	}
	
	# Did we skip a month and therefore the downloads have basically ended
	if( $trimdata ne "yes" ) {
		$scale = ScaleData();
		DoOutput($scale);
		ResetCounters();
	}

	#close the date file and generate image
	close(LOGDATA);
	close(OUT);
	$graphcmd = "./make-graphs-hist --firstdate \"$firstdate\" --detail downloadlogs --daily --input $outfile --output $image";
	print "About to execute this:\n";
	print "$graphcmd\n";
	system("$graphcmd");

}
exit(0);

# =================================
# print help
# =================================

sub help 
{
    print "Usage: writefile.pl --megs=#
Options:
	[-h/--help]				See this
	[-d/--data]				file holding test data
	\n";
}

# =================================
# Scale the data and return position info. Which
# data location is correct for color coding of data.
# =================================

sub ScaleData
{
	$total = $totalgood + $totalbad;
	if($total > 1000) {
		$total = $total / 200;
		$totalgood = $totalgood / 200;
		$totalbad = $totalbad / 200;
		$negbad = ( 0 - $totalbad);
		return(2);
	} elsif($total > 200) {
		$total = $total / 20;
		$totalgood = $totalgood / 20;
		$totalbad = $totalbad / 20;
		$negbad = ( 0 - $totalbad);
		return(1);
	}
	$negbad = ( 0 - $totalbad);
	return(0);
}

sub ResetCounters
{
	$total = 0;
	$totalgood = 0;
	$totalbad = 0;
}

sub DoOutput
{
	my $scale = shift;
	if( $scale == 0 ) {
		print OUT "$months{$month} $day $year, $totalgood, $negbad, 0, 0, 0, 0\n";
	} elsif( $scale == 1 ) {
		print OUT "$months{$month} $day $year, 0, 0, $totalgood, $negbad, 0, 0\n";
	} else { # 2 returned for scaling
		print OUT "$months{$month} $day $year, 0, 0, 0, 0, $totalgood, $negbad\n";
	}
}

sub CheckForEnd
{
	# $trimdata = CheckForEnd( $month, $2 );
	print "last month was $month and now $2\n";
	$lastmonth = shift;
	$thismonth = shift;
	$thisday = shift;
	$thisyear = shift; 
	# We have a bad data point where a Sept 2005 day falls in the middle
	# of january 2004 messing up my approximate trimming. Not worth
	# finding why as most of the data flows very smoothly
	if(($thismonth eq "Sep") && ($thisday eq "8") && ($thisyear eq "2005")) {
		return("no");
	}

	if(($thismonth eq "Nov") && ($thisday eq "25") && ($thisyear eq "2006")) {
		return("no");
	}

	if($lastmonth eq $thismonth) {
		return("no");
	}

	$lastmonth = $nummonths{$lastmonth};
	$thismonth = $nummonths{$thismonth};

	if( $lastmonth == 12 ) {
		if($thismonth == 1) {
			print "January after December\n";
			return("no");
		} else {
			print "NOT January after December\n";
			# end of month reversion to last month?
			# sometimes we have previous day events after
			# we change days....... assume this is the case 
			# and it is not an 11 month skip....
			if( $thismonth == 11 ) {
				# assume nov 30/Dec 1
				print "Late  november\n";
				return("no");
			} else {
				print "stopping on last <$lastmonth> this <$thismonth>\n";
				return("yes");
			}
		}
	} else {
		if( $thismonth != ($lastmonth + 1)) {
			print "Increment of NOT 1 $lastmonth/$thismonth\n";
			# end of month reversion to last month?
			# sometimes we have previous day events after
			# we change days....... assume this is the case 
			# and it is not an 11 month skip....
			if(($lastmonth == 1) && ($thismonth == 12)) {
				# dec 31/Jan 1
				print "dec 31/Jan 1\n";
				return("no");
			} elsif( $thismonth < $lastmonth ) {
				return("no");
				print "This less then last\n";
			} else {
				print "stopping on last <$lastmonth> this <$thismonth>\n";
				return("yes");
			}
		} else {
			print "Increment of 1 $lastmonth/$thismonth\n";
			return("no");
		}
	}
}
