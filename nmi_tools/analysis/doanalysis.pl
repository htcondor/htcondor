#!/usr/bin/perl
use File::Copy;
use File::Path;
use Getopt::Long;

my $datalocation = "/p/condor/public/html/developers/testsuite/";
my $data = $datalocation . "testblog_current.txt";

GetOptions (
		'help' => \$help,
		'data=s' => \$datafile,
		'plot' => \$plot,
		'start=s' => \$newwd,
);

if( $newwd ) { chdir("$newwd"); }
if ( $help )    { help() and exit(0); }

if( $datafile ) {
	#print "data file will be <<$datafile>>\n";
} else {
	$datafile = $datadefault;
}

$branch1 = "7_0";
$branch2 = "7_1";

#print "Transition file is <<7_0regdata>>\n";
$outfile = $datalocation . $branch1 . "regdata";
system("./readtestdata.pl --data=$data --view=testsum --branch=$branch1 --out=$outfile");
$outimage = $outfile . ".png";
system("./make-graphs  --daily --input $outfile --output $outimage");

#print "Transition file is <<7_0detaildata>>\n";
$outfile = $datalocation . $branch1 . "detaildata";
system("./readtestdata.pl --data=$data --view=testdetail  --branch=$branch1 --out=$outfile");
$outimage = $outfile . ".png";
system("./make-graphs  --daily --detail analdetail --input $outfile --output $outimage");

#print "Transition file is <<7_1regdata>>\n";
$outfile = $datalocation . $branch2 . "regdata";
system("./readtestdata.pl --data=$data --view=testsum --branch=$branch2 --out=$outfile");
$outimage = $outfile . ".png";
system("./make-graphs  --daily --input $outfile --output $outimage");

#print "Transition file is <<7_1detaildata>>\n";
$outfile = $datalocation . $branch2 . "detaildata";
system("./readtestdata.pl --data=$data --view=testdetail  --branch=$branch2 --out=$outfile");
$outimage = $outfile . ".png";
system("./make-graphs  --daily --detail analdetail --input $outfile --output $outimage");

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
