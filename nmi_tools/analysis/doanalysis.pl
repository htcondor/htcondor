#!/usr/bin/perl
use File::Copy;
use File::Path;
use Getopt::Long;

my $datalocation = "../testblog/";
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

$branch1 = "7_0_0";
$branch2 = "7_1";

print "Transition file is <<7_0regdata>>\n";
system("./readtestdata.pl --data=$data --view=testsum --branch=$branch1 --out=7_0regdata");
system("./make-graphs  --daily --input 7_0regdata --output ../testblog/7_0regdata.png");
exit(0);

print "Transition file is <<7_0detaildata>>\n";
system("./readtestdata.pl --data=$data --view=testdetail  --branch=$branch17_0 --out=7_0detaildata");
system("./make-graphs --detail analdetail --daily --input 7_0detaildata --output ../testblog/7_0detaildata.png");

print "Transition file is <<7_1regdata>>\n";
system("./readtestdata.pl --data=$data --view=testsum --branch=$branch2 --out=7_1regdata");
system("./make-graphs  --daily --input 7_1regdata --output ../testblog/7_1regdata.png");

print "Transition file is <<7_1detaildata>>\n";
system("./readtestdata.pl --data=$data --view=testdetail  --branch=$branch2 --out=7_1detaildata");
system("./make-graphs --detail analdetail --daily --input 7_1detaildata --output ../testblog/7_1detaildata.png");


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
