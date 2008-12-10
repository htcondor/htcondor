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
my $firstbranch = 1;

GetOptions (
		'help' => \$help,
		'start=s' => \$newwd,
);

if( $newwd ) { chdir("$newwd"); }
if ( $help )    { help() and exit(0); }

@branches = ("V7_2-branch", "trunk");
#@branches = ("V7_0-branch", "V7_1-trunk", "V7_2-branch", "trunk");
#@branches = ("trunk");
foreach $branch (@branches) {
	print "Anlyze branch $branch\n";
	$tmp_builddata = "/tmp/btplots/" . $branch . "autobuilds";
	$tmp_testdata = "/tmp/btplots/" . $branch . "autotests";
	$buildplacefile = $datalocation . $branch . "autobuilds.png";
	$testplacefile = $datalocation . $branch . "autotests.png";
	system("$toollocation/condor_readNMIdb.pl --type=builds --branch=$branch -d=2008-11-7");
	system("$toollocation/make-graphs --daily --input $tmp_builddata --output $buildplacefile --detail autobuilds");

	system("$toollocation/condor_readNMIdb.pl --type=tests --branch=$branch -d=2008-11-7");
	system("$toollocation/make-graphs --daily --input $tmp_testdata --output $testplacefile --detail autotests");

	if($firstbranch == 1) {
		system("$toollocation/condor_readNMIdb.pl --type=times --date=2008-11-7 --branch=$branch --save=$timefiles --new");
		$firstbranch = 0;
	} else {
		system("$toollocation/condor_readNMIdb.pl --type=times --date=2008-11-7 --branch=$branch --save=$timefiles --append");
	}
}

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
