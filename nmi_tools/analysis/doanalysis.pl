#!/usr/bin/perl
use File::Copy;
use File::Path;
use Getopt::Long;

my $datalocation = "../testblog/";
my $datadefault = "testblog_current.txt";
my $detailcmd = " --view=testdetail ";
my $regcmd = " --view=testsum";
my $crunchprog = "./readtestdata.pl";
my $imageprog = "./make-graphs";

my @branches = ("6_8", "6_9");
my @views = ( $regcmd, $detailcmd);
my %transfiles = (

    "1" => "6_8regdata",
    "2" => "6_8detaildata",
    "3" => "6_9regdata",
    "4" => "6_9detaildata",
);

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

my $count = 1;
foreach $branch (@branches) {
	#print "$branch\n";
	foreach	$view (@views) {
		print "Transition file is <<$transfiles{\"$count\"}>>\n";
		my $detail = " --detail analdetail";
		my $output = $transfiles{"$count"} . ".png";
		#$cmd = $crunchprog . " " . "--data=" . $datalocation . $datafile . " " . $view . " --branch=" . $branch . " --out=";
		if(!$plot) {
			$cmd = $crunchprog . " " . "--data=" . $datalocation . $datafile . " " . $view . " --branch=" . $branch . " --out=" . $transfiles{"$count"};
			print "$cmd\n";
			system("$cmd");
			if(!($transfiles{"$count"} =~ /detail/)) {
				$detail = " ";
			}
		}
		$mkimg = $imageprog . $detail . " --daily --input " .  $transfiles{"$count"} . " --output " . $datalocation . "/" . $output; 
		print "$mkimg\n";
		system("$mkimg");
		$count = $count + 1;
	}
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
