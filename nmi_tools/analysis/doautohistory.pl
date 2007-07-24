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

$placefile = $datalocation . "autobuilds.png";
system("/p/condor/home/tools/build-test-plots/condor_readNMIdb.pl --type=builds -d=2005-06-01");
system("/p/condor/home/tools/build-test-plots/mergeruns.pl --type=builds --data=/tmp/btplots/autobuilds");
system("/p/condor/home/tools/build-test-plots/make-graphs-hist --monthly --input /tmp/btplots/autobuilds --output $placefile --detail autobuilds");

$placefile = $datalocation . "autotests.png";
system("/p/condor/home/tools/build-test-plots/condor_readNMIdb.pl --type=tests -d=2005-06-01");
system("/p/condor/home/tools/build-test-plots/mergeruns.pl --type=tests --data=/tmp/btplots/autotests");
system("/p/condor/home/tools/build-test-plots/make-graphs-hist --monthly --input /tmp/btplots/autotests --output $placefile --detail autotests");
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
