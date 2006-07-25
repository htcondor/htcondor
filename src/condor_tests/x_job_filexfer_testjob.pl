#!/usr/bin/env perl

use File::Copy;
use File::Path;
use Getopt::Long;
use Cwd;

GetOptions (
	'help' => \$help,
	'noinput' => \$noinput,
	'noextrainput' => \$noextrainput,
	'extrainput' => \$extrainput,
	'onesetout' => \$onesetout,
	'long' => \$long,
	'forever' => \$forever,
	'job=i' => \$job,
	'failok' => \$failok,
);

if ( $help )    { help() and exit(0); }

my $workingdir = getcwd();
print "Working Directory is $workingdir\n";

# test basic running of script
if ( $noinput )
{
	print "Trivial success\n";
	exit(0)
}
else
{
	print "Trivial success\n";
}

my $Idir = "job_"."$job"."_dir";

# test for input = filenm
if ( $noextrainput )
{
	# test and leave
	my $Ifile = "submit_filetrans_input"."$job".".txt";
	print "Looking for input file $Ifile\n";
	system("ls submit_filetrans_input*");
	if( -f "$Ifile" )
	{
		print "Input file arrived\n";
		# reverse logic applies failing is good and working is bad. exit(0);
		if($failok)
		{
			exit(1);
		}
		else
		{
			exit(0);
		}
	}
	else
	{
		print "Input file failed to arrive\n";
		if($failok)
		{
			exit(0);
		}
		else
		{
			exit(1);
		}
	}
}


if( $extrainput )
{
	# test and leave
	my $Ifile1 = "submit_filetrans_input"."$job"."a.txt";
	my $Ifile2 = "submit_filetrans_input"."$job"."b.txt";
	my $Ifile3 = "submit_filetrans_input"."$job"."c.txt";
	if(!-f "$Ifile1")
	{ 
		print "$Ifile1 failed to arrive\n"; 
		if($failok)
		{
			exit(0);
		}
		else
		{
			exit(1);
		}
	}
	if(!-f "$Ifile2")
	{ 
		print "$Ifile2 failed to arrive\n"; 
		if($failok)
		{
			exit(0);
		}
		else
		{
			exit(1);
		}
	}
	if(!-f "$Ifile3")
	{ 
		print "$Ifile3 failed to arrive\n"; 
		if($failok)
		{
			exit(0);
		}
		else
		{
			exit(1);
		}
	}
	# test done leave
	print "Extra input files arrived\n";
		# reverse logic applies failing is good and working is bad. exit(0);
		if($failok)
		{
			exit(1);
		}
		else
		{
			exit(0);
		}
}

my $out = "submit_filetrans_output";

my $out1;
my $out2;
my $out3;
my $out4;
my $out5;
my $out6;

if( defined($job) )
{
	system("mkdir -p dir_$job");
	chdir("dir_$job");
	$out1 = "$out"."$job"."e.txt";
	$out2 = "$out"."$job"."f.txt";
	$out3 = "$out"."$job"."g.txt";
	$out4 = "$out"."$job"."h.txt";
	$out5 = "$out"."$job"."i.txt";
	$out6 = "$out"."$job"."j.txt";
}
else
{
	print "No processid given for output file naming\n";
	exit(1);
}


if( $onesetout )
{
	# create and leave
	system("touch $out1 $out2 $out3");
	system("cp * ..");
	# test done leave
	exit(0);
}
else
{
	# create 
	#system("touch $out1 $out2 $out3");
	system("ls ..  >$out1");
	system("ls ..  >$out2");
	system("ls ..  >$out3");
	system("cp * ..");

}

#allow time for vacate
sleep 20;

if( $long )
{
	# create and leave
	system("touch $out4 $out5 $out6");
	system("cp * ..");
	exit(0);
}
else
{
	# create 
	system("touch $out4 $out5 $out6");
	system("cp * ..");
}

if( $forever )
{
	while(1)
	{
		sleep(1);
	}
}

# =================================
# print help
# =================================

sub help
{
print "Usage: writefile.pl --megs=#
Options:
[-h/--help]                             See this
[--noinput]                             Only the script is needed, no checking
[--noextrainput]                        Only the script is needed and submit_filetrans_input.txt
\n";
}
