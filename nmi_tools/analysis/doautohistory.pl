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
#my $datalocation = "./";
my $toollocation = "/p/condor/home/tools/build-test-plots/";
my $timefiles = "/tmp/btplots/durationfiles";
my $tmpdir = "/tmp/btplots";
#my $tmpdir = "./tmp/";
my $rundir = "/p/condor/home/tools/build-test-plots";
#my $rundir = ".";
my $outfile = "";
my $infile = "";

GetOptions (
		'help' => \$help,
		'start=s' => \$newwd,
);

if( $newwd ) { chdir("$newwd"); }
if ( $help )    { help() and exit(0); }

$placefile = $datalocation . "projectautotests.png";
system("$rundir/condor_readNMIdb.pl --project --type=tests -d=2005-06-01");

$infile = "/tmp/btplots/projectautotests";
$outfile = $tmpdir . "projectautotests.avg";
system("$rundir/monthaverage.pl  --data=$infile --out=$outfile");

$infile = $tmpdir . "projectautotests.avg";
system("$rundir/make-graphs-hist --monthly --input $infile --output $placefile --detail autotests");

$placefile = $datalocation . "projectautobuilds.png";
system("$rundir/condor_readNMIdb.pl --project --type=builds -d=2005-06-01");

$infile = "/tmp/btplots/projectautobuilds";
$outfile = $tmpdir . "projectautobuilds.avg";
system("$rundir/monthaverage.pl  --data=$infile --out=$outfile");

$infile = $tmpdir . "projectautobuilds.avg";
system("$rundir/make-graphs-hist --monthly --input $infile --output $placefile --detail autobuilds");

$placefile = $datalocation . "autobuilds.png";
system("/p/condor/home/tools/build-test-plots/condor_readNMIdb.pl --type=builds -d=2005-06-01");

$outfile = $tmpdir . "autobuilds.merged";
system("/p/condor/home/tools/build-test-plots/mergeruns.pl --type=builds --data=/tmp/btplots/autobuilds --out=$outfile");

$infile = $tmpdir . "autobuilds.merged";
$outfile = $tmpdir . "autobuilds.avg";
system("/p/condor/home/tools/build-test-plots/monthaverage.pl  --data=$infile --out=$outfile");

$infile = $tmpdir . "autobuilds.avg";
system("/p/condor/home/tools/build-test-plots/make-graphs-hist --monthly --input $infile --output $placefile --detail autobuilds");

$placefile = $datalocation . "autotests.png";
system("/p/condor/home/tools/build-test-plots/condor_readNMIdb.pl.tst --type=tests -d=2005-06-01");

$outfile = $tmpdir . "autotests.merged";
system("/p/condor/home/tools/build-test-plots/mergeruns.pl --type=tests --data=/tmp/btplots/autotests --out=$outfile");

$infile = $tmpdir . "autotests.merged";
$outfile = $tmpdir . "autotests.avg";
system("/p/condor/home/tools/build-test-plots/monthaverage.pl  --data=$infile --out=$outfile");

$infile = $tmpdir . "autotests.avg";
system("/p/condor/home/tools/build-test-plots/make-graphs-hist --monthly --input $infile --output $placefile --detail autotests");
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
