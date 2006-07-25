#!/usr/bin/env perl

use Getopt::Long;
use Cwd;
use CondorTest;
use Condor;

my %params;
my %oldconfig;
my $figchanges;
my $protoconfig = "";
my $protoconfiglocal = "";
my $status = 0;
my $homedir = getcwd();
my $pathtoconfig = "";
my $personalmaster = "";
my $gendatafile = "";

my $scaletesting = "scaletesting.log";
#print "Trying to open logfile... $scaletesting\n";

#open(OLDOUT, ">&STDOUT");
#open(OLDERR, ">&STDERR");
#open(STDOUT, ">$scaletesting") or die "Could not open $scaletesting: $!";
#open(STDERR, ">&STDOUT");
#select(STDERR); $| = 1;
#select(STDOUT); $| = 1;

print "Scalability testing in location: $homedir\n";

GetOptions (
	'help' => \$help,
	'personal' => \$personal,
	'name=s' => \$name,
	'settings=s' => \$settings,
	'collector=s' => \$condorhost,
	'config=s' => \$newconfigfile,
	'start' => \$start,
);

my $runtemplate = "x_perf_autoscale_run_template";
my $cmdtemplate = "x_perf_autoscale_cmd_template";
my $runfile = "perf_";
my $cmdfile = "perf_";
my $runcore = "";

my $type = "";
my $universe = "";
my $duration = "";

my $startjobs = "";
my $incrementjobs = "";
my $stopjobs = "";

my $typename = "";
my $uniname = "";

if($help)
{
	help();
	exit(0);
}

if( $name )
{
	# customize the names
	$name = $name . "_";
}

print "Current run file now $runfile and cmdfile is $cmdfile\n";

if(! defined $settings )
{
	$settings = "x_perf_auto_paraam_template";
}

%params = parse_cmdfile( $settings );

if( !exists $params{"duration"} )
{
	die "Duration must be set in parameter file< $settings >!\n";
}

if( !exists $params{"type"} )
{
	die "Type must be set in parameter file< $settings >!\n";
}

if( !exists $params{"universe"} )
{
	die "Universe must be set in parameter file< $settings >!\n";
}

$type = $params{"type"};
$universe = $params{"universe"};
$duration = $params{"duration"};

if( $type eq "loading" )
{
	$typename = "busy_";
}
elsif( $type eq "jobs" )
{
	$typename = "jobs_";
}
elsif( $type eq "transfers" )
{
	$typename = "xfer_";
}
else
{
	die "Only jobs, transfers or loading allowed for type!\n";
}

if( $universe eq "vanilla" )
{
	$uniname = "van";
}
elsif( $universe eq "scheduler" )
{
	$uniname = "sched";
}
elsif( $universe eq "java" )
{
	$uniname = "java";
}
else
{
	die "Only vanilla, java or scheduler allowed for universe!\n";
}

print "Current run file now $runfile and cmdfile is $cmdfile\n";

# mandatory tests used in naming test output
# startjobs incrementjobs maxjobs

if(! exists $params{"startjobs"})
{
	print "startjobs not defined\n";
	return(1);
}
if(! exists $params{"incrementjobs"})
{
	print "incrementjobs not defined\n";
	return(1);
}
if(! exists $params{"maxjobs"})
{
	print "maxjobs not defined\n";
	return(1);
}

$startjobs = $params{"startjobs"};
$incrementjobs = $params{"incrementjobs"};
$stopjobs = $params{"maxjobs"};
my $incname = $startjobs . "_" . $incrementjobs . "_" . $stopjobs . "_" ;

# customize the names
$runfile = $runfile . $typename . $name . $incname . $uniname . ".run";
$cmdfile = $cmdfile . $typename . $name . $incname . $uniname . ".cmd";
$runcore = "perf_" . $typename . $name . $incname . $uniname;

print "Current run file now $runfile and cmdfile is $cmdfile\n";

if( !defined $condorhost )
{
	if( ! exists $params{"collector"} )
	{
		my $tmp = `/bin/hostname`;
		chomp($tmp);
		print "Guessing pool to be run on $tmp\n";
		$condorhost = $tmp;
		#die "Where is the collector to be? Can not generate condor_config!\n";
	}
	else
	{
		$condorhost = $params{"collector"};
	}
}

if( !defined $newconfigfile )
{
	$newconfigfile = "x_condor_config_template";
}

$pathtoconfig = $homedir . "/" . $newconfigfile;
print "Path to config file is $pathtoconfig\n";

foreach $key  (sort keys %params)
{
	#print "Key = $key\n";
}

# NOTE:
# So at this point we have the entire configuration inside
# the hash %params. Each stage of configuring the test will
# make demands of what it expects to be defined at the time and what
# defaults if any will be used.

$status = gen_autoscale_cmd( %params );
if($status != 0)
{
	die "Failed to generate cmd file!\n";
}
else
{
	print "Done generating cmd file!\n";
}

$status = gen_autoscale_run( %params );
if($status != 0)
{
	die "Failed to generate run file!\n";
}
else
{
	print "Done generating run file!\n";
}

my $condorstate = "";

if( $start )
{
	# we are running in the desired environment so fire it off.
	print "Running the test now!.................................\n";
	system("$runfile");
}

# =================================
# print help
# =================================

sub help
{
	print "Usage: writefile.pl --megs=#
	Options:
		[-h/--help]                             See this
		[-n/--name=s]								Mandatory naming parameter.
		[-p/--personal]                         Generate a personal condor to run within
		[-col/--collector=s]                      Configure pool around this host
		[-con/--config=s]                         Write configuration to what file?
		[-st/--start]                         	Start test after creating test.
		[-set/--settings=s]                       Change the default parameter file
		\n";
}

sub gen_autoscale_cmd
{
	my %control = @_;
	my $arguments;
	my $executable;

	# mandatory tests
	if(! exists $control{"universe"})
	{
		print "universe not defined\n";
		return(1);
	}
	if(! exists $control{"type"})
	{
		print "type not defined\n";
		return(1);
	}

	# Process old command file and extract and toss executable
	# and arguments having started with the combination requested based
	# on "type".

	if( !exists $control{"duration"} )
	{
		$duration = 1;
	}

	$gendatafile = $runcore . ".data";
	print "gendata now --$gendatafile--\n";

	# get a new submit file started
	open(NEWCMDFILE,">$cmdfile") || die "Runfile not around: $!\n";
	print NEWCMDFILE "universe = $universe\n";

	if( $type eq "jobs" )
	{
		if( $universe eq "java" )
		{
			print NEWCMDFILE "executable = Sleep.class\n";
			print NEWCMDFILE "arguments = Sleep $duration\n";
		}
		elsif(( $universe eq "vanilla" ) || ( $universe eq "scheduler" ))
		{
			print NEWCMDFILE "executable = x_sleep.pl\n";
			print NEWCMDFILE "arguments = $duration\n";
		}
		else
		{
			die "Unsupported universe in submitfile generation: $universe\n";
		}
	}
	elsif( $type eq "transfers" )
	{
		if( ! -e "transfers/data" )
		{
			my $curdir = getcwd();
			system("mkdir -p transfers");
			chdir("transfers");
			system("../transfer_createdata.pl --megs=10");
			chdir("$curdir");
		}

		if( $universe eq "java" )
		{
			print NEWCMDFILE "executable = CopyFile.class\n";
			print NEWCMDFILE "arguments = CopyFile data backdata\n";
			print NEWCMDFILE "transfer_input_files = transfers/data\n";
		}
		elsif(( $universe eq "vanilla" ) || ( $universe eq "scheduler" ))
		{
			print NEWCMDFILE "executable = x_perf_transfer_remote.pl\n";
			print NEWCMDFILE "arguments = backdata\n";
			print NEWCMDFILE "transfer_input_files = transfers/data\n";
		}
		else
		{
			die "Unsupported universe in submitfile generation: $universe\n";
		}
	}
	elsif( $type eq "loading" )
	{
		if( $universe eq "java" )
		{
			print NEWCMDFILE "executable = Busy.class\n";
			print NEWCMDFILE "arguments = Busy $duration\n";
		}
		elsif(( $universe eq "vanilla" ) || ( $universe eq "scheduler" ))
		{
			print NEWCMDFILE "executable = x_perf_busy.pl\n";
			print NEWCMDFILE "arguments = $duration\n";
		}
		else
		{
			die "Unsupported universe in submitfile generation: $universe\n";
		}
	}
	else
	{
		die "TYPE configuration is unacceptable!(jobs/transfers/loading): $type\n";
	}

	open(CMDFILE,"<$cmdtemplate") || die "Cmdfile not around: $!\n";
	while(<CMDFILE>)
	{
		chomp();
		$line = $_;
		if( $line =~ /^\s*universe\s*=\s*.*$/ )
		{
			#print "FOUND universe!!!!!\n";
		}
		elsif( $line =~ /^\s*arguments\s*=\s*.*$/ )
		{
			#print "FOUND arguments!!!!!\n";
		}
		elsif( $line =~ /^\s*executable\s*=\s*.*$/ )
		{
			#print "FOUND executable!!!!!\n";
		}
		elsif( $line =~ /^\s*transfer_input_files\s*=\s*.*$/ )
		{
			#print "FOUND executable!!!!!\n";
		}
		elsif( $line =~ /^\s*output\s*=\s*.*$/ )
		{
			print NEWCMDFILE "output = $runcore.out\n";
		}
		elsif( $line =~ /^\s*error\s*=\s*.*$/ )
		{
			print NEWCMDFILE "error = $runcore.err\n";
		}
		elsif( $line =~ /^\s*log\s*=\s*.*$/ )
		{
			print NEWCMDFILE "log = $runcore.log\n";
		}
		else
		{
			print NEWCMDFILE "$line\n";
		}
	}
	close(CMDFILE);
	close(NEWCMDFILE);
	system("chmod 755 $cmdfile");
}

sub gen_autoscale_run
{
	my %control = @_;


	# edit run file and insert parameters
	my $line;

	open(RUNFILE,"<$runtemplate") || die "Runfile not around: $!\n";
	open(NEWRUNFILE,">$runfile") || die "Runfile not around: $!\n";

	while(<RUNFILE>)
	{
		chomp();
		$line = $_;
		if( $line =~ /startjobs\s*=/ )
		{
			print "FOUND startjobs!!!!!\n";
			my $strtcnt = $control{"startjobs"};
			print NEWRUNFILE "my \$startjobs = $strtcnt;\n";
		}
		elsif( $line =~ /incrementjobs\s*=/ )
		{
			print "FOUND incrementjobs!!!!!\n";
			my $strtcnt = $control{"incrementjobs"};
			print NEWRUNFILE "my \$incrementjobs = $strtcnt;\n";
		}
		elsif( $line =~ /maxjobs\s*=/ )
		{
			print "FOUND maxjobs!!!!!\n";
			my $strtcnt = $control{"maxjobs"};
			print NEWRUNFILE "my \$maxjobs = $strtcnt;\n";
		}
		elsif( $line =~ /datafile\s*=/ )
		{
			print "FOUND datafile!!!!!\n";
			print NEWRUNFILE "my \$datafile = \"$gendatafile\";\n";
		}
		elsif( $line =~ /cmdfile\s*=/ )
		{
			print "FOUND cmdfile!!!!!\n";
			print NEWRUNFILE "my \$cmdfile = \"$cmdfile\";\n";
		}
		elsif( $line =~ /CondorTest::StartCondor/ )
		{
			if( $personal )
			{
				print "Personal Condor Request ON !!\n";
				print NEWRUNFILE "CondorTest::StartCondor($settings);\n";
			}
			else
			{
				print "Personal Condor Request OFF !!\n";
				print NEWRUNFILE "#CondorTest::StartCondor($settings);\n";
			}
		}
		else
		{
			print NEWRUNFILE "$line\n";
		}
	}
	close(RUNFILE);
	close(NEWRUNFILE);
	system("chmod 755 $runfile");
}

# opens given filename and returns hash of attr/value pairs
#
# usage:
# my (%hash) = parse_cmdfile("filename");
#

sub parse_cmdfile()
{
	my $infile = shift( @_ );
	my %cmd;
	my $linenum = 0;
	
	print "Reading parameters from $infile\n";

	open( INFILE, "<", $infile ) || die "error opening file \"$infile\": $!\n";

	while( <INFILE> )
	{
		# keep track of line numbers
		$linenum++;
		# skip blank lines
		next if( /^\s*$/ );
		# skip comments
		next if( /^\s*\#/ );
		# remove trailing newline
		chomp();

		#print "Examining $_\n";

		# parse attr/value pair
		if( /^\s*(\S+)\s*\=\s*(.*)\s*$/ )
		{
			$cmd{$1} = $2;
			print "Loading parameter $1 with value $2\n";
		}
		else
		{
			print STDERR "$infile:$linenum ignored due to syntax error\n";
		}
	}

	close( INFILE ) || die "error closing file \"$infile\": $!\n";
	return %cmd;
}
