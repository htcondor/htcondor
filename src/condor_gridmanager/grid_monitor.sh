#! /usr/bin/env perl

# Sends results from the grid_manager_monitor_agent back to a
# globus-url-copy friendly URL whenever those results are updated.
# If necessary, starts the grid_manager_monitor_agent (packaged here as
# the __DATA__ segment).
#
# Expected user is the Condor-G grid-manager which will start this to
# monitor the status of jobs on a Globus gatekeeper.


$^W = 1; # same as -w, but -w doesn't work quite run under /usr/bin/env hack
use strict;

use POSIX ":sys_wait_h";
use Fcntl;

$| = 1;

# How old can the status file get before we don't want to use it any more.
# In seconds.
# --max-status-age=SECONDS
my $MAX_STATUS_FILE_AGE = 60;

# Time is seconds between when an agent is started and we expect
# results.  If the results don't show up, we assume something went wrong
# and send off another agent
#
# (Note: we don't kill the agent even if it exceeds the time, it may yet
# get back to us.  The newly started agent should detect the existing
# agent and silently bail out.)
my $MAX_AGENT_START_BEFORE_OUTPUT = 60*2;

# What do we call the agent?  Used in user messages.
my $GRID_AGENT_NAME = 'grid_manager_monitor_agent';

# Location to send the file to.  You probably do NOT want to
# specify a default here, since it should be set on the command line.
# --dest-url=STRING
my $DST_URL = undef;

# Maximum time this program should exist before terminating.
# In seconds
# --max-lifespan=SECONDS
my $MAX_LIFE = 60 * 60;

# How long are we allowed to wait between keepalives?
# seconds
# --min-update-interval=SECONDS
my $MIN_TIME_BETWEEN_UPDATES = 60;

# Don't try restarting without seeing any results more than
# this many times.  If undef, is unused.
my $MAX_SEQUENTIAL_GRID_AGENT_STARTS = undef;

# Set to 1 to use Greenwich Mean Time
# --use-gmt
my $USE_GMT = 0;

# Arguments to pass to the grid agent when spawned.
my(@GRID_AGENT_ARGS) = (
	'--delete-self',
);


# Error codes:
	# Max lifetime exceeded.
my $ERROR_MAX_LIFETIME = 0;
	# GLOBUS_LOCATION not configured
my $ERROR_NO_GLOBUS_LOCATION = 1;
	# Problem with status file.
my $ERROR_BAD_STATUS_FILE = 2;
	# Never got results from grid_manager_monitor_agent
my $ERROR_NO_RESULTS = 3;
	# Got unexpected or ill-specified command line argument
my $ERROR_BAD_CMD_LINE_ARG = 4;
	# Tried and failed to start the grid_manager_monitor_agent
my $ERROR_NO_START_AGENT = 5;
	# Attempt to send file back using globus-url-copy failed.
my $ERROR_GUC_FAILED = 6;
	# the grid_manager_monitor_agent exited with a 0 code.
#my $ERROR_AGENT_EXITED_OK = 7; # no longer used.
	# the grid_manager_monitor_agent exited with a non-zero code.
my $ERROR_AGENT_EXITED_BAD = 8;



# Holds copy of the agent in case we need to install and start it.
my $AGENT_SOURCE;
{
	local $/ = undef;
	$AGENT_SOURCE = <DATA>;
}

# Alway start an agent at the beginning.
my $force_agent_start = 1;

# Hold PID of last started agent.
my $last_started_agent_pid = 0;


sub REAPER {
	my $child;

	# Normally we'd test for -1, but 0 can mean "processes running".
	# waitpid returns:
	#  -1 = No process exitted
	#  0  = Processes running (some systems, including Linux)
	#  >0 = Pid that exitted.
	if (($child = waitpid(-1, WNOHANG)) <= 0) {
		# system() call.  We get the signal, but not the wait.

		# Probably no necessary, but just in case we're on a crusty SysV
		# box, reset the signal handler.
		$SIG{'CHLD'} = \&REAPER;
		return;
	}
	my $result = $?;
	my $return = $result / 256;

	if($child != $last_started_agent_pid) {
		report_info("Unknown child process pid $child exited with"
			."$return result ($result).");
		$SIG{'CHLD'} = \&REAPER;
		return;
	}

	my $AGENT_RESULT_TIMEOUT = 0;
	my $AGENT_RESULT_ALREADY_RUNNING = 1;
	if($return == $AGENT_RESULT_TIMEOUT) {
		# The agent ran out of time.  Restart it.
		$force_agent_start = 1;
		report_info("$GRID_AGENT_NAME timed out.  Restarting");
		$SIG{'CHLD'} = \&REAPER;
		return;
	}
	if($return == $AGENT_RESULT_ALREADY_RUNNING) {
		# That's good, silently continue.
		report_info("$GRID_AGENT_NAME already running.");
		$SIG{'CHLD'} = \&REAPER;
		return;
	}
	fatal_error( $ERROR_AGENT_EXITED_BAD,
		"$GRID_AGENT_NAME (pid $child) exited with a $return result ($result).");
}

$SIG{'CHLD'} = \&REAPER;

my(@argv) = @ARGV;
while(@argv) {
	my $arg = shift @argv;
	if      ($arg =~ /^--max-lifespan=(\d+)$/) {
		$MAX_LIFE = $1;
	} elsif ($arg =~ /^--dest-url=(\S+)$/) {
		$DST_URL = $1;
	} elsif ($arg =~ /^--min-update-interval=(\d+)$/) {
		$MIN_TIME_BETWEEN_UPDATES = $1;
	} elsif ($arg =~ /^--max-status-age=(\d+)$/) {
		$MAX_STATUS_FILE_AGE = $1;
	} elsif ($arg =~ /^--use-gmt$/) {
		$USE_GMT = 1;
	} else {
		fatal_error($ERROR_BAD_CMD_LINE_ARG,
			"Unknown or malformed argument '$arg'");
	}
}

if(not defined $DST_URL) {
	fatal_error($ERROR_BAD_CMD_LINE_ARG, "You MUST specify --dest-url");
}

my $OLD_AGE = time() + $MAX_LIFE;
if($MAX_LIFE == 0) {
	# 2003-03-20 + 10 years.
	$OLD_AGE = 1048207714 + 60*60*24*265*10;
}

if(not exists $ENV{GLOBUS_LOCATION}) {
	fatal_error($ERROR_NO_GLOBUS_LOCATION, "GLOBUS_LOCATION not available.  Unable to locate status file.");
}

my $username = (getpwuid($>))[0];

my $STATUS_FILE = "$ENV{GLOBUS_LOCATION}/tmp/${GRID_AGENT_NAME}_log.$>";

# Have encountered situations where LD_LIBRARY_PATH isn't set.
# This shouldn't happen, but it's been seen.
print "$ENV{LD_LIBRARY_PATH}\n";
{
	my $gl = $ENV{GLOBUS_LOCATION};
	if(exists $ENV{'LD_LIBRARY_PATH'} 
		and defined $ENV{'LD_LIBRARY_PATH'}
		and length $ENV{'LD_LIBRARY_PATH'}) {
		$ENV{'LD_LIBRARY_PATH'} .= ':';
	} else {
		$ENV{'LD_LIBRARY_PATH'} = '';
	}
	$ENV{'LD_LIBRARY_PATH'} .= "$ENV{GLOBUS_LOCATION}/lib";
}

my($last_grid_file_timestamp) = 0;


my $next_update_time = 0;
while(1)
{
	report_ok(); # used as keepalive

	if(time() > $OLD_AGE) {
		fatal_error($ERROR_MAX_LIFETIME,
			"Maximum lifetime ($MAX_LIFE seconds) exceeded.");
	}

	$next_update_time = time() + $MIN_TIME_BETWEEN_UPDATES;

	if(not -e $STATUS_FILE) {
		start_agent($last_grid_file_timestamp, "$STATUS_FILE missing");
		next; # skip to next try, see if file exists.
	}

	my($file_uid, $this_grid_file_timestamp) = (stat($STATUS_FILE))[4,9];

	if( $file_uid != $> ) {
		fatal_error($ERROR_BAD_STATUS_FILE,
			"$> ($username) doesn't own $STATUS_FILE");
	}

	my $now = time();
	my $old_age = $this_grid_file_timestamp + $MAX_STATUS_FILE_AGE;
	if( $last_grid_file_timestamp == $this_grid_file_timestamp and
		$old_age < $now ) {
		start_agent($last_grid_file_timestamp,
			"$STATUS_FILE looks old ($this_grid_file_timestamp more "
			."than $MAX_STATUS_FILE_AGE second older than $now)");
		next;
	}

	if($force_agent_start) {
		start_agent($last_grid_file_timestamp, "Forced agent start");
	}


	if($last_grid_file_timestamp == $this_grid_file_timestamp) {
		# unchanged
		next;
	}

	$last_grid_file_timestamp = $this_grid_file_timestamp;

	# The file is fresh enough to use.
	send_file($STATUS_FILE, $DST_URL);

} continue {
	my $sleep_length = $next_update_time - time();
	sleep $sleep_length if $sleep_length > 0;
}


################################################################################
sub send_file {
	my($src_file, $dst_url) = @_;
	my $src_url = "file://$src_file";
	my $ret = system("$ENV{GLOBUS_LOCATION}/bin/globus-url-copy", $src_url, $dst_url);
	if($ret == -1) {
		fatal_error($ERROR_GUC_FAILED,
			"Failed to execute globus-url-copy $src_url $dst_url "
			."because of $!");
	}
	if($ret != 0) {
		my $real_ret = $ret / 256;
		fatal_error($ERROR_GUC_FAILED,
			"globus-url-copy $src_url $dst_url failed for unknown reason. "
			."wait status is $ret, return value might be $real_ret");
	}
}


################################################################################
BEGIN {

	# Local static variables for sub start_agent
my $last_grid_agent_start = 0;
my $num_sequential_grid_agent_starts = 0;

sub start_agent {
	my $last_grid_file_timestamp = shift;
	report_info(@_) if @_;
	report_info("Starting $GRID_AGENT_NAME");

	$force_agent_start = 0;

	# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 
	# It's possible that we just started the agent, we just
	# haven't seen any results.  Throttle start up rate.
	my $next_allowed_restart =
		$MAX_AGENT_START_BEFORE_OUTPUT + $last_grid_agent_start;
	if( $last_grid_agent_start > 0 and $next_allowed_restart > time())
	{
		my $delta = time() - $last_grid_agent_start;
		my $next = timestamp($next_allowed_restart);
		report_info("Skipping restart.  $GRID_AGENT_NAME last started $delta seconds ago.  Will not restart until $next.");
		return;
	}

	# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 
	# Make sure we're not just looping, trying and failing to start
	# the grid agent over and over.
	if($last_grid_file_timestamp < $last_grid_agent_start) {
		# We never got results from last start;
		$num_sequential_grid_agent_starts++
	} else {
		$num_sequential_grid_agent_starts = 1;
	}
	if( defined $MAX_SEQUENTIAL_GRID_AGENT_STARTS and
		$num_sequential_grid_agent_starts > $MAX_SEQUENTIAL_GRID_AGENT_STARTS ) {
		fatal_error($ERROR_NO_RESULTS,
			"Started $GRID_AGENT_NAME $num_sequential_grid_agent_starts "
			."times, but never saw output.");
	}

	# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 
	# Dump grid agent into a file to run.
	$last_grid_agent_start = time();

	my $MAX_TMP_TRIES = 1000;
	local *FH;
	my $counter = 0;
	my $filename;
	my $username = (getpwuid($>))[0];
	do {
		$filename = "/tmp/$GRID_AGENT_NAME.$username.$$.$MAX_TMP_TRIES";
		$MAX_TMP_TRIES--;
	} until ( sysopen(FH, $filename, O_RDWR|O_CREAT|O_EXCL, 0744) or
		($MAX_TMP_TRIES < 1) );

	if($MAX_TMP_TRIES < 1) {
		fatal_error($ERROR_NO_START_AGENT,
			"Unable to create temporary file for $GRID_AGENT_NAME");
	}

	print FH $AGENT_SOURCE;
	close FH;
	chmod 0755, $filename
		or fatal_error($ERROR_NO_START_AGENT,
			"Unable to set permissions on temporary file for "
			."$GRID_AGENT_NAME ($filename)");

	my $fork_result = fork();
	if(not defined $fork_result) {
		fatal_error($ERROR_NO_START_AGENT,
			"Unable to fork to start $GRID_AGENT_NAME");
	}

	if($fork_result == 0) { # child
		exec $filename, @GRID_AGENT_ARGS, "--maxtime=${MAX_LIFE}s";
		fatal_error($ERROR_NO_START_AGENT, "Failed to exec $filename");
		#TODO: My parent will muddle on, perhaps kill it?
	} else { # parent
		report_info("Started $GRID_AGENT_NAME as $filename, pid $fork_result");
		$last_started_agent_pid = $fork_result;
	}
}

}

################################################################################
sub fatal_error {
	my($num) = shift;
	print timestamp()." ERROR: $num: @_\n";
	exit $num;
}

################################################################################
sub report_ok {
	print timestamp()." OK: @_\n";
}

################################################################################
sub report_info {
	print timestamp()." INFO: @_\n";
}

################################################################################
sub timestamp {
	my $time = time();
	$time = shift if @_;
	my($sec,$min,$hour,$mday,$mon,$year,$wday,$yday);
	if($USE_GMT) {
		($sec,$min,$hour,$mday,$mon,$year,$wday,$yday) = gmtime($time);
	} else {
		($sec,$min,$hour,$mday,$mon,$year,$wday,$yday) = localtime($time);
	}
	$year += 1900;
	$mon += 1;
	return sprintf "%04d-%02d-%02d %02d:%02d:%02d",
		$year, $mon, $mday, $hour, $min, $sec;
}

################################################################################
# The actual grid_manager_monitor_agent should follow the __DATA__
__DATA__
#! /usr/bin/env perl
use strict;
use IO::File;
use File::stat;
use Errno;

# Enable warnings
$^W = 1;

# Prototypes
sub FindStateDirs( );
sub ReadJobs( );
sub RunCheck( );
sub ParseSeconds( $$ );
sub ParseStateText( $$ );
sub CheckJob ( $ );
sub CreateJobDescription( $ );
sub CreateJobManager( $$ );
sub GetJobState( $ );
sub Alive( );
sub Death( );

# Set signal handler..
$SIG{TERM} = 'Death';

# Config info
my $OutputName = "grid_manager_monitor_agent_log.$<";
my %Config = (
	      RunMode		=> "die",
	      Period		=> 60,
	      MaxRunTime	=> 999999999,
	      OutputFile	=> "",
	      TouchState	=> 1,
	      Debug		=> 0,
	      CheckUid		=> 1,
	     );

# Timestamp of the last "alive" mark
my $LastAlive = -1;

# Command line handling...
foreach my $Arg ( @ARGV )
{
    if ( $Arg =~ /^--dir=(.+)/ )
    {
	foreach my $Dir ( split ( /,/, $1 ) )
	{
	    $Config{StateFileDirs}{$Dir} = 1;
	}
    }
    elsif ( $Arg =~ /^--debug(=(\d+))?/ )
    {
	if ( defined $2 )
	{
	    $Config{Debug} = $2;
	}
	else
	{
	    $Config{Debug}++;
	}
    }
    elsif ( $Arg =~ /^--file=(.+)/ )
    {
	$Config{OutputFile} = $1;
    }
    elsif ( $Arg =~ /^--period=(\d+)([sSmMhHdD]?)/ )
    {
	$Config{Period} = ParseSeconds( $1, $2 );
    }
    elsif ( $Arg =~ /^--mode=(die|kill)$/ )
    {
	$Config{RunMode} = $1;
    }
    elsif ( $Arg =~ /^--touch=(yes|no)$/i )
    {
	$Config{TouchState} = $1 =~ /yes/i;
    }
    elsif ( $Arg =~ /^--checkuid=(yes|no)$/i )
    {
	$Config{CheckUid} = $1 =~ /yes/i;
    }
    elsif ( $Arg =~ /^--maxtime=(\d+)([sSmMhHdD]?)$/ )
    {
	$Config{MaxRunTime} = ParseSeconds( $1, $2 );
    }
    elsif ( $Arg =~ /^--delete-self$/ )
    {
	unlink( $0 );
    }
    else
    {
	print STDERR "Unknown arg '$Arg'\n" if ( ! ( $Arg =~ /^--help$/ ) );
	print STDERR "Usage: $0 [options]\n";
	print STDERR "    [--help]               Dump out this help\n";
	print STDERR "    [--file=name]          ".
	    "Specify the state summary file to generate <auto>\n";
	print STDERR "    [--dir=dir[,dir..]]    Specify the state file ".
	    "directory(s) to search <auto>\n";
	print STDERR "    [--debug[=level]       Specify debug level (0=off) <0>\n";
	print STDERR "    [--period=time[smhd]]  Specify the period polls <60s>\n";
	print STDERR "    [--maxtime=time[smhd]] Specify the maximum run time <forever>\n";
	print STDERR "    [--mode=(kill|die)]    Kill=Kill other polls; ".
	    "die=die if others are running <die>\n";
	print STDERR "    [--touch=(yes|no)]     Touch state files? <yes>\n";
	print STDERR "    [--checkuid=(yes|no)]  Check the uid of the state files? <yes>\n";
	exit 2;
    }
}
$#ARGV = -1;
$| = 1;

# Notify the debugging user...
print "Debug level $Config{Debug}\n" if ( $Config{Debug} );

# Verify that a valid GLOBUS_LOCATION is specified
die "No globus locaion env variable"
    if ( ! exists $ENV{GLOBUS_LOCATION} );
die "GLOBUS_LOCATION=$ENV{GLOBUSLOCATION} is invalid"
    if ( ! -d $ENV{GLOBUS_LOCATION} );

# Set the default ouptut file if none specified
$Config{OutputFile} = "$ENV{GLOBUS_LOCATION}/tmp/$OutputName"
    if ( $Config{OutputFile} eq "" );

# Setup the lock file..
$Config{LockFile} = $Config{OutputFile} . ".lock";
$Config{TmpFile} = $Config{OutputFile} . ".tmp";

# Are we alone?
RunCheck( );

# Find the state file directories...
if ( ! exists $Config{StateFileDirs} )
{
    $Config{StateFileDirs} = FindStateDirs( );
}

# Make sure Globus is in our perl search path..
BEGIN { push( @INC, "$ENV{GLOBUS_LOCATION}/lib/perl" ) };
print "Perl search path set to " . join( ":", @INC ) . "\n"
    if ( $Config{Debug} );

# The Job Description class definition
use Globus::GRAM::JobDescription;

# The various JobManager derived classes
my %AvailableJobTypes;
foreach my $JobType ( qw( condor fork lsf pbs remote ) )
{
    my $Module = "Globus::GRAM::JobManager::$JobType";
    print "Trying $JobType => $Module\n" if ( $Config{Debug} > 2 );
    eval "require $Module";
    print "Error = $@\n" if ( $@ && $Config{Debug} > 3 );
    next if $@;
    print "Enabling $JobType\n" if ( $Config{Debug} > 2 );
    import $Module;
    $AvailableJobTypes{$JobType} = 1;
}

# The Job State definitions
use Globus::GRAM::JobState;

print "Supported job types = " . join( ", ", keys %AvailableJobTypes ) . "\n"
    if ( $Config{Debug} );

# Quick bit of feedback...
if ( $Config{Debug} )
{
    print "Poll: Max Run Time = $Config{MaxRunTime}s; Poll Period = $Config{Period}s\n";
    foreach my $Dir ( keys %{$Config{StateFileDirs}} )
    {
	print "State File Directory: $Dir\n";
    }
}

# Loop forever...
my $QuitTime = time( ) + $Config{MaxRunTime};
while( 1 )
{
    # Get the current time...
    my $StartTime = time( );
    last if ( $StartTime >= $QuitTime );
    print "Starting sampling @ " . localtime( $StartTime ) . "\n"
	if ( $Config{Debug} > 1 );

    # Mark time for next run
    my $NextTime = $StartTime + $Config{Period};

    # And, do this run
    ReadJobs( );

    # Sleep for the next run (if time's not up)
    if ( $Config{Debug} > 1 )
    {
	my $EndTime = time( );
	print "Finished sampling @ " . localtime( $EndTime ) . "\n";
	print "Elapsed time = " . ( $EndTime - $StartTime  ) . "\n";
    }

    # Loop til it's time for the next pass
    while ( time( ) < $NextTime )
    {
	sleep( 1 );
	Alive( );
    }

}
exit 0;

# If no state file directory was specified, let's try to build our own list..
sub FindStateDirs( )
{
    my $GridServices = "$ENV{GLOBUS_LOCATION}/etc/grid-services";
    my %Dirs;


    # Walk through the grid services..
    if ( -d $GridServices )
    {
	my %JmConfigs;

	opendir( DIR, $GridServices );
	foreach my $JmFile ( grep( /^jobmanager/, readdir( DIR ) ) )
	{
	    if ( open( JMFILE, "$GridServices/$JmFile" ) )
	    {
		while( <JMFILE> )
		{
		    if ( /-conf\s+(\S+)/ )
		    {
			$JmConfigs{$1} = 1;
		    }
		}
		close( JMFILE );
	    }
	}

	# Walk through the Job Manager configs now..
	foreach my $JmConfig ( keys %JmConfigs )
	{
	    open( JMCONFIG, "$JmConfig" ) ||
		die "Can't read Job Manager config '$JmConfig'";
	    while( <JMCONFIG> )
	    {
		if ( /-state-file-dir\s+(\S+)/ )
		{
		    $Dirs{$1} = 1;
		}
	    }
	    close( JMCONFIG );
	}
    }

    # Nothing found?  Check $GLOBUS_LOCATION/tmp/gram_job_state..
    if ( 0 == scalar ( keys %Dirs ) )
    {
	my $Dir = $ENV{GLOBUS_LOCATION} . "/tmp/gram_job_state";
	$Dirs{$Dir} = 1	if ( -d $Dir );
    }

    # Make sure that we found *something*
    if ( 0 == scalar ( keys %Dirs ) )
    {
	print STDERR "No state file directories found!\n";
	exit 2;
    }

    # Done...
    return \%Dirs;
}

# Finally, let's walk through the whole list...
sub ReadJobs( )
{
    # Record the current time..
    my $PassStartTime = time( );

    my %Jobs;
    foreach my $StateFileDir ( keys %{$Config{StateFileDirs}} )
    {
	# State files
	my @StateFiles;

	# Update the lock file mtime..
	Alive( );

	# Read the directory
	opendir( DIR, $StateFileDir ) || die "Can't read state file directory ";
	while( my $File = readdir( DIR ) )
	{
	    # Skip the lock files
	    next if ( $File =~ /\.lock$/ );
	    next if ( $File eq "." || $File eq ".." );
	    next if ( $File =~ /^gram_condor_log/ );

	    # And, stuff it in the list
	    push @StateFiles, $File

	}
	closedir( DIR );

	# Now, let's walk through the list of state files
      READSTATE:
	foreach my $StateFile ( @StateFiles )
	{
	    Alive( );
	    my @Text;	# Text from the file..

	    # The full path to the file...
	    my $FullPath = "$StateFileDir/$StateFile";

	    # Loop 'til we read successfully...
	    my $Done = 0;
	    my $Tries = 0;
	    my $Owner = "";
	    while( ! $Done )
	    {
		# Give up after a lot of tries
		if ( $Tries++ > 10 )
		{
		    warn "Giving up on '$FullPath'\n";
		    last;
		}

		# If it's gone already, just skip to the next...
		next READSTATE if ( ! -f $FullPath );

		# Stat the file; we need it's mtime & uid
		my $Stat = File::stat::stat( $FullPath );
		if ( ( $Config{CheckUid} ) && ( @{$Stat}[4] != $< ) )
		{
		    next READSTATE;
		}

		# Record the file's mtime at the start of reading it...
		my $StartTime = @{$Stat}[9];

		# Open the file...
		if ( ! open( FILE, $FullPath ) )
		{
		    warn "Can't open state file $StateFile";
		    next READSTATE;
		}

		# Read it
		@Text = <FILE>;
		chomp @Text;

		# Done reading
		close( FILE );

		# Simple validity check
		redo if ( $#Text < 10 );

		# And, recheck the mtime...
		$Stat = File::stat::stat( $FullPath );
		my $EndTime = @{$Stat}[9];

		# If the mtime has changed, go read it again...
		redo if ( $StartTime != $EndTime );

		# Extract the file's UID
		$Owner = getpwuid( @{$Stat}[4] );

		# Note the we're done!
		$Done = 1;
	    }

	    # If we never got "done", skip this file...
	    next if ( ! $Done );

	    # Hash to store our job info in..
	    my %Job;

	    # First, store off the text verbatim..
	    $Job{StateFile} = $StateFile;
	    $Job{FullPath} = $FullPath;

	    # Store the contents away
	    ParseStateText( \%Job, \@Text, );
	    my $Contact = $Job{ContactString};
	    if ( $Config{Debug} > 4 )
	    {
		print "StateFile = $StateFile, Contact = '$Contact', JobId = $Job{JobId}\n";
	    }

	    # If we don't know the job manager type, skip it..
	    next if ( ! CheckJob( \%Job ) );

	    # Set the spool directory env
	    local $ENV{GLOBUS_SPOOL_DIR};
	    $Owner = "" if ( ! defined $Owner );
	    if ( ( $StateFile =~ /^$Owner\./ ) ||
		 ( ( $Owner eq "" ) && ( !($StateFile =~ /^job\./ )) )  )
	    {
		# If -state-file-dir specified in the config,
		# Do nothing
	    }
	    else
	    {
		# Otherwise, set the env var.
		$ENV{GLOBUS_SPOOL_DIR} = $StateFileDir;
	    }

	    # Debugging output..
	    if ( $Config{Debug} > 4 )
	    {
		my $GSD = $ENV{GLOBUS_SPOOL_DIR};
		$GSD = "<undefined>" if ( ! defined $GSD );
		print "GLOBUS_SPOOL_DIR = '$GSD'\n";
	    }

	    # Create a job description and job manager
	    my $JobDescription = CreateJobDescription( \%Job );
	    my $JobManager = CreateJobManager( \%Job, $JobDescription );

	    # And, get the current state as best we can..
	    my $JobState = GetJobState( $JobManager );

	    # Store it..
	    if ( defined $JobState )
	    {
		$Jobs{$Contact} = $JobState;

		# And, update its mtime..
		if ( $Config{TouchState} )
		{
		    my $Now = time( );	
		    utime( $Now, $Now, $Job{FullPath} );
		}
	    }
	}
    }

    # Write out the file...
    Alive( );
    my $File = $Config{OutputFile};
    my $TmpFile = $Config{TmpFile};
    open( FILE, ">$TmpFile" ) || die( "Can't write to data file '$TmpFile': $!" );
    print FILE "$PassStartTime " . time() . "\n";
    foreach my $Job ( sort keys %Jobs )
    {
	printf FILE "%-60s %5d\n", $Job, $Jobs{$Job};
    }
    close( FILE );
    print "Wrote " . scalar( keys %Jobs ) . " entries to $TmpFile\n"
	if ( $Config{Debug} );

    # Ok, move it on top of the real file
    rename( $TmpFile, $File ) || die "Rename '$TmpFile' -> '$File' failed: $!";
}

# Check to see if we're the only process running, handle it...
sub RunCheck( )
{
    my $LockFile = $Config{LockFile};
    my $LockPid = -1;		# PID from the lock file
    my $LockTime = -1;		# Time from the lock file
    my $PollPid = -1;		# PID of runing poll program..

    # Check for & read the lock file...
    if ( open( LOCK, $LockFile ) )
    {
	$_ = <LOCK>;
	if ( /(\d+) (\d+)/ )
	{
	    $LockPid = $1;
	    $LockTime = $2;
	}
	close( LOCK );
	if ( ( $LockPid <= 0 ) || ( $LockTime <= 0 ) )
	{
	    warn "Invalid lock file '$LockFile'";
	}
    }

    # See if the process is alive...
    if ( $LockPid > 0 )
    {
	# Does it exit?
	if ( kill( 0, $LockPid ) )
	{
	    # Ok, it exists... Is the lock file recent?
	    my $Stat = File::stat::stat( $LockFile );
	    my $LockMtime = @{$Stat}[9];

	    # Ok, let's grab a time 2 minutes in the past..
	    my $MinTime = time() - 120;

	    # If the file's mtime is recent, assume that $LockPid
	    # is alive and is the process that modified it.
	    if ( $LockMtime > $MinTime )
	    {
		$PollPid = $LockPid;
	    }
	}
    }

    # If we're in "die" mode, give up if another process is running
    if ( ( $Config{RunMode} eq "die" ) && ( $PollPid > 0 ) )
    {
	print STDERR "$PollPid is already running\n";
	exit( 1 );
    }

    # Ok, now we're in "kill mode"; kill the other process
    if ( $PollPid > 0 )
    {
	# Send it a friendly SIGTERM
	if ( ! kill( "TERM", $PollPid ) )
	{
	    die "Can't kill $PollPid: $!" if ( ! $!{ESRCH} );
	}

	# Wait for it to die...
	for my $Delay ( 0 .. 10 )
	{
	    last if ( ! kill( 0, $PollPid ) );
	    sleep( 1 );
	}

	# If it's still alive, send a less friendly SIGKILL...
	if ( kill( 0, $PollPid ) )
	{
	    if ( ( ! kill( "KILL", $PollPid ) ) && ( ! $!{ESRCH} ) )
	    {
		die "Can't kill $PollPid: $!";
	    }
	}
    }

    # Remove the old lock file..
    if ( ! unlink( $LockFile ) )
    {
	die "Can't unlink lock file: $!" if ( ! $!{ENOENT} );
    }

    # Ok, now let's try to create the lock ourselves...
    my $TempFile = $LockFile . ".$$";
    my $Now = time( );
    open( LOCK, ">$TempFile" ) || die "Can't create my temp lock file $TempFile: $!";
    print LOCK "$$ $Now\n";
    close( LOCK );

    # Initialize the alive marker
    $LastAlive = $Now;

    # And, rename it.  If we win, we're the champs.  If we lose, give up.
    if ( ! link( $TempFile, $LockFile ) )
    {
	unlink( $TempFile );
	print STDERR "Error creating lock file $LockFile $!\nGiving up\n";
	exit 2;
    }

    # Ok, we won!!
    unlink( $TempFile );

    # Done
    return 0;
}

# Parse a seconds expression (i.e. 1s or 5m)
sub ParseSeconds( $$ )
{
    my $Seconds = shift;
    my $Modifier = shift;

    if ( ( ! defined $Modifier ) || ( $Modifier eq "" ) ||
	 ( $Modifier eq "s" ) || ( $Modifier eq "S" ) )
    {
	# Do nothing
    }
    elsif ( ( $Modifier eq "m" ) || ( $Modifier eq "M" ) )
    {
	$Seconds *= 60;
    }
    elsif ( ( $Modifier eq "h" ) || ( $Modifier eq "H" ) )
    {
	$Seconds *= (60 * 60);
    }
    elsif ( ( $Modifier eq "d" ) || ( $Modifier eq "D" ) )
    {
	$Seconds *= (60 * 60 * 24);
    }

    # Done; return it
    $Seconds;
}

# Parse the state file array
sub ParseStateText( $$ )
{
    my $JobRef = shift;
    my $StateText = shift;

    # Store the stae text verbatim...
    $JobRef->{FullState} = [ @{$StateText} ];

    # And, clean it all up.
    my @CleanText;
    foreach my $Text ( @{$StateText} )
    {
	$Text =~ s/^\s+//;
	push @CleanText, $Text;
    }

    # Now, pull stuff out of the "cleaned" text of the state file...
    $JobRef->{ContactString} = shift @CleanText;
    $JobRef->{JobManagerStatus} = shift @CleanText;
    $JobRef->{Status} = shift @CleanText;
    $JobRef->{FailureCode} = shift @CleanText;
    $JobRef->{JobId} = shift @CleanText;
    $JobRef->{RslSpec} = shift @CleanText;
    $JobRef->{CacheTag} = shift @CleanText;
    $JobRef->{JobManagerType} = shift @CleanText
	if ( !( $CleanText[0] =~ /^\d+$/ ) );
    $JobRef->{TwoPhaseCommit} = shift @CleanText;
    $JobRef->{ScratchDir} = shift @CleanText;

    # Return the hash
    return $JobRef;
}

# Check for things in the job hash
sub CheckJob ( $ )
{
    my $JobRef = shift;
    my $JobId = $JobRef->{JobId};

    # Verify that it's got a job manager type.  Can't proceed without it.
    if ( ! ( exists $JobRef->{JobManagerType} ) ||
	 ( $JobRef->{JobManagerType} eq "" ) )
    {
	print "$JobId: $JobRef->{ContactString}: Job manager type unknown\n"
	    if ( $Config{Debug} > 2 );
	return 0;
    }

    # If we don't know about this job manager type, skip it..
    my $JobType = $JobRef->{JobManagerType};
    if ( ! exists $AvailableJobTypes{$JobType} )
    {
	print "$JobId: Job type '$JobType' is unsupported\n"
	    if ( $Config{Debug} > 2 );
	return 0;
    }

    # Looks ok here.
    return 1;

}

# Create a Job Description thingy
sub CreateJobDescription( $ )
{
    my $JobRef = shift;

    # Get the job type
    my $JobType = $JobRef->{JobManagerType};
    my $Contact = $JobRef->{ContactString};

    # Generate the "uniq id"
    my $UniqId;
    {
	my @Tmp = split( /\//, $Contact );
	my $Id2 = pop @Tmp;
	my $Id1 = pop @Tmp;
	$UniqId = $Id1 . "." . $Id2;
    }

    # No jobid?  Means it hasn't been submitted to the local sched yet.
    if ( $JobRef->{JobId} eq "" )
    {
	print "No JobId\n" if ( $Config{Debug} > 2 );
	return undef;
    }

    # Extract the job count
    my $JobCount = 1;
    if ( $JobRef->{RslSpec} =~ /\(\s*\"count\"\s*=\s*\"?(\d+)\"?\s+\)/ )
    {
	$JobCount = $1;
    }

    # Ok, now let's contact the local sched.
    # Start by creating a "JobDescription" object
    my %TmpDescription = (
			  jobid => [ $JobRef->{JobId}, ],
			  uniqid => [ $UniqId, ],
			  count => [ $JobCount, ],
			  cachetag => [ $JobRef->{CacheTag}, ],
			  scratchdir => [ $JobRef->{ScratchDir}, ],
			 );
    my $JobDescription = new Globus::GRAM::JobDescription( \%TmpDescription );

    # Done.
    return $JobDescription
}

# Create a Job Manager thingy
sub CreateJobManager( $$ )
{
    my $JobRef = shift;
    my $JobDescription = shift;

    # Make sure the job description is defined...
    if ( ! defined $JobDescription )
    {
	print "Invalid job description\n" if ( $Config{Debug} > 2 );
	return undef;
    }

    # Get the job type
    my $JobType = $JobRef->{JobManagerType};

    # Ok, now create the job manager object
    my $ManagerClass = "Globus::GRAM::JobManager::$JobType";
    my $JobManager = new $ManagerClass( $JobDescription );

    return $JobManager;
}

# Get the job's current state.
sub GetJobState( $ )
{
    my $JobManager = shift;

    # Job is unsubmitted if there is no Job Manager object
    return Globus::GRAM::JobState::UNSUBMITTED if ( ! defined $JobManager );

    # Get the job's status
    my $JobStatus = $JobManager->poll( );

    # If the state is defined, we can store it..
    if ( ( ref $JobStatus eq "HASH" ) && ( defined $JobStatus->{JOB_STATE} )  )
    {
	return $JobStatus->{JOB_STATE};
    }

    # Otherwise, undef
    print "Job state unknown\n" if ( $Config{Debug} > 2 );
    return undef;
}

# Touch the lock file to indicate that I'm alive & well.
sub Alive( )
{
    # And, update its mtime..
    my $Now = time( );	
    if ( $Now > ( $LastAlive + 5 ) )
    {
	print "Touching $Config{LockFile} -> $Now\n" if ( $Config{Debug} > 2 );
	utime( $Now, $Now, $Config{LockFile} );
	$LastAlive = $Now;
    }
}

# Handle a TERM signal
sub Death( )
{
    print "Term signal; cleaning up\n" if ( $Config{Debug} );
    unlink( $Config{TmpFile} );
    unlink( $Config{LockFile} );
    exit 0;
}
