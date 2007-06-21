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


print "Transition file is <<6_8regdata>>\n";
system("./readtestdata.pl --data=$data --view=testsum --branch=6_8 --out=6_8regdata");
system("./make-graphs  --daily --input 6_8regdata --output ../testblog/6_8regdata.png");
print "Transition file is <<6_8detaildata>>\n";
system("./readtestdata.pl --data=$data --view=testdetail  --branch=6_8 --out=6_8detaildata");
system("./make-graphs --detail analdetail --daily --input 6_8detaildata --output ../testblog/6_8detaildata.png");
print "Transition file is <<6_9regdata>>\n";
system("./readtestdata.pl --data=$data --view=testsum --branch=6_9 --out=6_9regdata");
system("./make-graphs  --daily --input 6_9regdata --output ../testblog/6_9regdata.png");
print "Transition file is <<6_9detaildata>>\n";
system("./readtestdata.pl --data=$data --view=testdetail  --branch=6_9 --out=6_9detaildata");
system("./make-graphs --detail analdetail --daily --input 6_9detaildata --output ../testblog/6_9detaildata.png");


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
