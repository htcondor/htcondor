#!/s/perl/bin/perl
use File::Copy;
use File::Path;
use Getopt::Long;

system("mkdir -p /scratch/bt/btplots");

my $dbtimelog = "/scratch/bt/btplots/autoplot.log";
#print "Trying to open logfile... $dbtimelog\n";
open(OLDOUT, ">&STDOUT");
open(OLDERR, ">&STDERR");
open(STDOUT, ">$dbtimelog") or die "Could not open $dbtimelog: $!";
open(STDERR, ">&STDOUT");
select(STDERR);
 $| = 1;
select(STDOUT);
 $| = 1;

my $timefiles = "/scratch/bt/btplots/durationfiles";

my $datalocation = "/p/condor/public/html/developers/testsuite/";
#my $datalocation = "./";

my $rundir = "/p/condor/home/tools/build-test-plots";
#my $rundir = ".";

my $tmpdir = "/scratch/bt/btplots/";
#my $tmpdir = "./tmp/";

my $outfile = "";
my $infile = "";

GetOptions (
		'help' => \$help,
		'start=s' => \$newwd,
		'autobuilds' => \$autobuilds,
		'autotests' => \$autotests,
		'testsperbuild' => \$testsperbuild,
		'projecttests' => \$projecttests,
		'projectbuilds' => \$projectbuilds,
		'all' => \$all,
);

if( $newwd ) { chdir("$newwd"); }
if ( $help )    { help() and exit(0); }

#AUTO BUILDS
if(defined $autobuilds || defined $all) {
	$placefile = $datalocation . "autobuilds.png";
	system("$rundir/condor_readNMIdb.pl --type=builds -d=2005-07-01");

	$outfile = $tmpdir . "autobuilds.merged";
	system("$rundir/mergeruns.pl --type=builds --data=/scratch/bt/btplots/autobuilds --out=$outfile");

	$infile = $tmpdir . "autobuilds.merged";
	$outfile = $tmpdir . "autobuilds.avg";
	system("$rundir/monthaverage.pl  --data=$infile --out=$outfile");

	$infile = $tmpdir . "autobuilds.avg";
	system("$rundir/make-graphs-hist --monthly --input $infile --output $placefile --detail autobuilds");
}
#AUTO TESTS
if(defined $autotests || defined $all) {
	$placefile = $datalocation . "autotests.png";
	system("$rundir/condor_readNMIdb.pl --type=tests -d=2005-07-01");

	print "merging from /scratch/bt/btplots/autotests to $outfile\n";
	$outfile = $tmpdir . "autotests.merged";
	system("$rundir/mergeruns.pl --type=tests --data=/scratch/bt/btplots/autotests --out=$outfile");

	$infile = $tmpdir . "autotests.merged";
	$outfile = $tmpdir . "autotests.avg";

	print "merged data here $infile averaged data here $outfile\n";
	system("$rundir/monthaverage.pl  --data=$infile --out=$outfile");

	$infile = $tmpdir . "autotests.avg";
	system("$rundir/make-graphs-hist --monthly --input $infile --output $placefile --detail autotests");
}

#TESTS PER BUILD
if(defined $testsperbuild || defined $all) {
	$placefile = $datalocation . "testsperbuild.png";
	$avgbuilds = $tmpdir . "autobuilds.avg";
	$avgtests = $tmpdir . "autotests.avg";
	$out = $tmpdir . "testsperbuild.avg";
	system("$rundir/testaverage.pl --builds=$avgbuilds --tests=$avgtests --out=$out");

	$infile = $tmpdir . "testsperbuild.avg";
	system("$rundir/make-graphs-hist --monthly --input $infile --output $placefile --detail testsperplatform");
}

# PROJECT TESTS
if(defined $projecttests || defined $all) {
	$placefile = $datalocation . "projectautotests.png";
	system("$rundir/condor_readNMIdb.pl --project --type=tests -d=2005-07-01");

	$infile = "/scratch/bt/btplots/projectautotests";
	$outfile = $tmpdir . "projectautotests.avg";
	system("$rundir/monthaverage.pl  --data=$infile --out=$outfile");

	$infile = $tmpdir . "projectautotests.avg";
	system("$rundir/make-graphs-hist --monthly --input $infile --output $placefile --detail autotests");
}

#PROJECT BUILDS
if(defined $projectbuilds || defined $all) {
	$placefile = $datalocation . "projectautobuilds.png";
	system("$rundir/condor_readNMIdb.pl --project --type=builds -d=2005-07-01");

	$infile = "/scratch/bt/btplots/projectautobuilds";
	$outfile = $tmpdir . "projectautobuilds.avg";
	system("$rundir/monthaverage.pl  --data=$infile --out=$outfile");

	$infile = $tmpdir . "projectautobuilds.avg";
	system("$rundir/make-graphs-hist --monthly --input $infile --output $placefile --detail autobuilds");
}

#
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
