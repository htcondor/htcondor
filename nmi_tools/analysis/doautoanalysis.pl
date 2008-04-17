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
my $toollocation = "/p/condor/home/tools/build-test-plots";
#my $toollocation = ".";
my $timefiles = "/tmp/btplots/durationfiles";

GetOptions (
		'help' => \$help,
		'start=s' => \$newwd,
);

$branch1 = "7_0";
$branch2 = "7_1";
$tmp_builddata1 = "/tmp/btplots/" . $branch1 . "autobuilds";
$tmp_builddata2 = "/tmp/btplots/" . $branch2 . "autobuilds";
$tmp_testdata1 = "/tmp/btplots/" . $branch1 . "autotests";
$tmp_testdata2 = "/tmp/btplots/" . $branch2 . "autotests";

if( $newwd ) { chdir("$newwd"); }
if ( $help )    { help() and exit(0); }

$placefile = $datalocation . "V7_0autobuilds.png";
system("$toollocation/condor_readNMIdb.pl --type=builds --branch=$branch1 -d=2007-12-01");
system("$toollocation/make-graphs --daily --input $tmp_builddata1 --output $placefile --detail autobuilds");

$placefile = $datalocation . "V7_1autobuilds.png";
system("$toollocation/condor_readNMIdb.pl --type=builds --branch=$branch2 -d=2007-12-01");
system("$toollocation/make-graphs --daily --input $tmp_builddata2 --output $placefile --detail autobuilds");

$placefile = $datalocation . "V7_0autotests.png";
system("$toollocation/condor_readNMIdb.pl --type=tests --branch=$branch1 -d=2007-12-01");
system("$toollocation/make-graphs --daily --input $tmp_testdata1 --output $placefile --detail autotests");

$placefile = $datalocation . "V7_1autotests.png";
system("$toollocation/condor_readNMIdb.pl --type=tests --branch=$branch2 -d=2007-12-01");
system("$toollocation/make-graphs --daily --input $tmp_testdata2 --output $placefile --detail autotests");

system("$toollocation/condor_readNMIdb.pl --type=times --date=2007-12-10 --branch=$branch1 --save=$timefiles --new");
system("$toollocation/condor_readNMIdb.pl --type=times --date=2007-12-10 --branch=$branch2 --save=$timefiles --append");

# due to the api change and the name use change of platform we
# must take the newer data and place with the older data
system("$toollocation/blendplatforms.pl");

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
	system("$toollocation/make-graphs --daily --input $current --output $placefile --detail times");
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
