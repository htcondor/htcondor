#!/s/perl/bin/perl
use File::Copy;
use File::Path;
use Getopt::Long;

system("mkdir -p /tmp/btplots");

my $dbtimelog = "/tmp/btplots/autoplot.log";
#print "Trying to open logfile... $dbtimelog\n";
open(OLDOUT, ">&STDOUT");
open(OLDERR, ">&STDERR");
open(STDOUT, ">$dbtimelog") or die "Could not open $dbtimelog: $!";
open(STDERR, ">&STDOUT");
select(STDERR);
 $| = 1;
select(STDOUT);
 $| = 1;

my $datalocation = "/p/condor/public/html/developers/testsuite/";
my $toollocation = "/p/condor/home/tools/build-test-plots/";
my $timefiles = "/tmp/btplots/durationfiles";

GetOptions (
		'help' => \$help,
		'start=s' => \$newwd,
);


if( $newwd ) { chdir("$newwd"); }
if ( $help )    { help() and exit(0); }

$placefile = $datalocation . "V6_8autobuilds.png";
system("/p/condor/home/tools/build-test-plots/condor_readNMIdb.pl --type=builds --branch=V6_8 -d=2007-05-01");
system("/p/condor/home/tools/build-test-plots/make-graphs --daily --input /tmp/btplots/V6_8autobuilds --output $placefile --detail autobuilds");
$placefile = $datalocation . "V6_9autobuilds.png";
system("/p/condor/home/tools/build-test-plots/condor_readNMIdb.pl --type=builds --branch=V6_9 -d=2007-05-01");
system("/p/condor/home/tools/build-test-plots/make-graphs --daily --input /tmp/btplots/V6_9autobuilds --output $placefile --detail autobuilds");

$placefile = $datalocation . "V6_8autotests.png";
system("/p/condor/home/tools/build-test-plots/condor_readNMIdb.pl --type=tests --branch=V6_8 -d=2007-05-01");
system("/p/condor/home/tools/build-test-plots/make-graphs --daily --input /tmp/btplots/V6_8autotests --output $placefile --detail autotests");
$placefile = $datalocation . "V6_9autotests.png";
system("/p/condor/home/tools/build-test-plots/condor_readNMIdb.pl --type=tests --branch=V6_9 -d=2007-05-01");
system("/p/condor/home/tools/build-test-plots/make-graphs --daily --input /tmp/btplots/V6_9autotests --output $placefile --detail autotests");

system("/p/condor/home/tools/build-test-plots/condor_readNMIdb.pl --type=times --date=2007-05-10 --branch=6_8 --save=$timefiles --new");
system("/p/condor/home/tools/build-test-plots/condor_readNMIdb.pl --type=times --date=2007-05-10 --branch=6_9 --save=$timefiles --append");

open(TIMEDFILES,"<$timefiles") || die "Can not find timed data<$timefiles>: $!\n";
$current = "";
$relocate = "";
while(<TIMEDFILES>) {
	chomp();
	$current = $_;
	#print "Time files <<$current>>\n";
	if($current =~ /^\/.*\/(.*)$/) {
		$relocate = $1;
		#print "striping $current to $relocate\n";
	} else {
		$relocate = $current;
	}
	$placefile = $datalocation . $relocate . ".png";
	#print "Plot $current to $relocate\n";
	system("/p/condor/home/tools/build-test-plots/make-graphs --daily --input $current --output $placefile --detail times");
}

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
