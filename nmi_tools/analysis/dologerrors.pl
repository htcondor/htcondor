#!/usr/bin/perl
use File::Copy;
use File::Path;
use Getopt::Long;
use Time::Local;

system("mkdir -p /tmp/btplots");

my $dbtimelog = "/tmp/btplots/dologerrors.log";
#print "Trying to open logfile... $dbtimelog\n";
open(OLDOUT, ">&STDOUT");
open(OLDERR, ">&STDERR");
open(STDOUT, ">$dbtimelog") or die "Could not open $dbtimelog: $!";
open(STDERR, ">&STDOUT");
select(STDERR);
 $| = 1;
 select(STDOUT);
  $| = 1;

chdir("/p/condor/public/html/developers/download_stats");

my $tooldir = "/p/condor/home/tools/build-test-plots/";
#my $tooldir = ".";
my $datalocation = "/p/condor/public/license/";
#my @datafiles = ( "sendfile-v6.0", "sendfile-v6.1", "sendfile-v6.2", "sendfile-v6.3", "sendfile-v6.4", "sendfile-v6.5", "sendfile-v6.6", "sendfile-v6.7", "sendfile-v6.8", "sendfile-v6.9","sendfile-v7.0", "sendfile-v7.1" );
#my @datafiles = ( "sendfile-v6.6", "sendfile-v6.7", "sendfile-v6.8", "sendfile-v6.9","sendfile-v7.0", "sendfile-v7.1" );
#my @datafiles = ( "sendfile-v6.8", "sendfile-v6.9", "sendfile-v7.0", "sendfile-v7.1" );
my @datafiles = ( "problem-v7.0" );
#my @datafiles = ( "problem-v7.0", "problem-v7.1" );
#my @datafiles = ( "sendfile-v6.8" );
%datapoints; 

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

my %previousmonth = (
    "Feb" => "January",
    "Mar" => "February",
    "Apr" => "March",
    "May" => "April",
    "Jun" => "May",
    "Jul" => "June",
    "Aug" => "July",
    "Sep" => "August",
    "Oct" => "September",
    "Nov" => "October",
    "Dec" => "November",
    "Jan" => "December",
);

my %nextmonth = (
    "Dec" => "January",
    "Jan" => "February",
    "Feb" => "March",
    "Mar" => "April",
    "Apr" => "May",
    "May" => "June",
    "Jun" => "July",
    "Jul" => "August",
    "Aug" => "September",
    "Sep" => "October",
    "Oct" => "November",
    "Nov" => "December",
);

my %monthdays = (
    "Jan" => 31,
    "Feb" => 28,
    "Mar" => 31,
    "Apr" => 30,
    "May" => 31,
    "Jun" => 30,
    "Jul" => 31,
    "Aug" => 31,
    "Sep" => 30,
    "Oct" => 31,
    "Nov" => 30,
    "Dec" => 31,
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

my %monthtoregnum = (
    "January" => 1,
	"February" => 2,
	"March" => 3,
    "April" => 4,
    "May" => 5,
    "June" => 6,
    "July" => 7,
    "August" => 8,
    "September" => 9,
    "October" => 10,
    "November" => 11,
    "December" => 12,
);

my %monthtonum = (
    "January" => 0,
	"February" => 1,
	"March" => 2,
    "April" => 3,
    "May" => 4,
    "June" => 5,
    "July" => 6,
    "August" => 7,
    "September" => 8,
    "October" => 9,
    "November" => 10,
    "December" => 11,
);

my %monthnums = (
    1 => "Jan",
    2 => "Feb",
    3 => "Mar",
    4 => "Apr",
    5 => "May",
    6 => "Jun",
    7 => "Jul",
    8 => "Aug",
    9 => "Sep",
    10 => "Oct",
    11 => "Nov",
    12 => "Dec",
);

if ( $help )    { help() and exit(0); }

ScanDownloadErrors ("problem-v7.0",\%datapoints);

DropData();
exit(0);

sub ScanDownloadErrors 
{
	my $log = shift;
	my $hashref = shift;
	$datafile = $datalocation . $log;
	print "examine $datafile\n";
	$newrecord = 0;

	open(LOGDATA,"<$datafile") || die "logdata bad: $!\n";
	my $line = "";
	my @error = ();
	my $skiprecord = 1;
	my $firstreset = 1;

	my $date = "";
	while(<LOGDATA>) {
		chomp;
		$line = $_;
        #unshift @queue, $_;
		print "$_\n";
		if($line =~ /^[\-]+----$/) {
			print "New record\n";
			if($skiprecord == 0) {
				print "Process Last Record\n";
			} else {
				if($firstreset == 0) {
					# remove count when first seen
					${$hashref}{"$date"} -= 1;
				} else {
					$firstreset = 0;
				}
			}
			@error = ();
			$skiprecord = 1;
		} elsif($line =~ /^(\d+)\s+(\d+\.\d+\.\d+\.\d+).*$/) {
			print "$1 $2\n";
			my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime($1);
			$mon += 1;
			$year += 1900;
			print "$months{$monthnums{$mon}} $mday $year\n";
			$date = "$months{$monthnums{$mon}} $mday $year";
			${$hashref}{"$date"} += 1;
		} elsif($line =~ /^UM:(.*)$/) {
			# some people sent a blank form - do not count as error
			if($1 eq "") {
				print "Content not defined\n";
			} else {
				$skiprecord = 0;
			}
		}
		push @error, $line;
    }
	

}



sub DropData 
{
	foreach  $key (sort keys %datapoints) {
		if(exists $datapoints{$key}) {
			print "$key $datapoints{$key} \n";
		}
	}
}

sub help 
{
    print "Usage: writefile.pl --megs=#
Options:
	[-h/--help]				See this
	[-d/--data]				file holding test data
	\n";
}
