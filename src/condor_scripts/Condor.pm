# Condor.pm - a Perl API to Condor
#
# 19??-???-?? originally written by stanis (?)
# 2000-Jun-02 Total overhaul by pfc@cs.wisc.edu and wright@cs.wisc.edu
# 2004-July-07 Add detecting $ENV(foo) in submit file parseing and fetch
#	from the environment before saving attribute pair.

# NOTE: currently works with only one cluster at a time (i.e., you can
# submit & monitor many clusters in series, but not in parallel).
# This won't be hard to fix, but it's best done using Perl
# quasi-objects and I'm not familiar enough with them to do it quickly
# and have a roaring deadline to meet...sorry.  --pfc

package Condor;

require 5.0;
use Carp;
use FileHandle;
use POSIX "sys_wait_h";

BEGIN
{
    $CONDOR_SUBMIT = 'condor_submit';
    $CONDOR_SUBMIT_DAG = 'condor_submit_dag';
    $CONDOR_VACATE = 'condor_vacate';
    $CONDOR_RESCHD = 'condor_reschedule';

    $DEBUG = 1;
    $cluster = 0;
    $num_active_jobs = 0;
    $saw_submit = 0;
    %submit_info;
	%machine_ads;

	$submit_time = 0;
	$TimedCallbackWait = 0;

}

sub Reset
{
    $cluster = 0;
    $num_active_jobs = 0;
    $saw_submit = 0;
    %submit_info = {};
    %machine_ads = {};

	$submit_time = 0;
	$TimedCallbackWait = 0;

    undef $SubmitCallback;
    undef $ExecuteCallback;
    undef $EvictedCallback;
    undef $EvictedWithCheckpointCallback;
    undef $EvictedWithRequeueCallback;
    undef $EvictedWithoutCheckpointCallback;
    undef $ExitedCallback;
    undef $ExitedSuccessCallback;
    undef $ExitedFailureCallback;
    undef $ExitedAbnormalCallback;
    undef $AbortCallback;
    undef $ShadowCallback;
    undef $HoldCallback;
    undef $ReleaseCallback;
    undef $JobErrCallback;
    undef $TimedCallback;
    undef $WantErrorCallback;
}

# submits job/s to condor, recording all variables from the submit
# file in global %submit_info
#
# - takes the name of a submit file as an argument
# - returns 0 upon failure, or cluster id upon success
sub Submit
{
    my $cmd_file = shift || croak "missing submit file argument";

    # reset global state
    Reset();

    # submit the file
    runCommand( "$CONDOR_SUBMIT $cmd_file > $cmd_file.out 2>&1" )
	|| return 0;

	$submit_time = time; # time reference for time callback

    # snarf the cluster id from condor_submit's output
    unless( open( SUBMIT, "<$cmd_file.out" ) )
    {
	warn "error opening \"$cmd_file.out\": $!\n";
	return 0;
    }
    while( <SUBMIT> )
    {
	if( /\d+ job\(s\) submitted to cluster (\d+)./ )
	{
	    $cluster = $1;
	    last;
	}
    }
    close SUBMIT;

    # if for some reason we didn't find the cluster id 
    unless( $cluster )
    {
	warn "error: couldn't find cluster id in condor_submit output\n";
	return 0;
    }
    
    # snarf info from the submit file and save for future reference
    unless( ParseSubmitFile( $cmd_file ) )
    {
	print "error: couldn't correctly parse submit file $cmd_file\n";
	return 0;
    }

    return $cluster;
}

# submits job/s to condor dagman
#
# - takes the name of a dag file as an argument
# - returns 0 upon failure, or cluster id upon success
sub SubmitDagman
{
    my $cmd_file = shift || croak "missing Dag file argument";
	my $cmd_args = shift || croak "missing Dagman Args";

    # reset global state
    Reset();

    # submit the file
    runCommand( "$CONDOR_SUBMIT_DAG $cmd_args -f -notification never $cmd_file > $cmd_file.out 2>&1" )
	|| return 0;

	$submit_time = time; # time reference for time callback

    # snarf the cluster id from condor_submit's output
    unless( open( SUBMIT, "<$cmd_file.out" ) )
    {
	warn "error opening \"$cmd_file.out\": $!\n";
	return 0;
    }
    while( <SUBMIT> )
    {
	if( /\d+ job\(s\) submitted to cluster (\d+)./ )
	{
	    $cluster = $1;
		debug("Cluster is $cluster\n");
	    last;
	}
    }
    close SUBMIT;

    # if for some reason we didn't find the cluster id 
    unless( $cluster )
    {
	warn "error: couldn't find cluster id in condor_submit output\n";
	return 0;
    }
    
    # snarf info from the submit file and save for future reference
	my $dagsubmitfile = $cmd_file  . ".condor.sub";
    unless( ParseSubmitFile( $dagsubmitfile ) )
    {
	print "error: couldn't correctly parse submit file $dagsubmitfile\n";
	return 0;
    }

    return $cluster;
}

# submits job/s to condor, recording all variables from the submit
# file in global %submit_info
#
# - takes the name of a submit file as an argument
# - returns 0 upon failure, or cluster id upon success
sub TestSubmit
{
    my $cmd_file = shift || croak "missing submit file argument";

    # reset global state
    # Reset();
	# Reset done in CondorTest.pm prior to submit

    # submit the file
    runCommand( "$CONDOR_SUBMIT $cmd_file > $cmd_file.out 2>&1" )
	|| return 0;

	$submit_time = time; # time reference for time callback

    # snarf the cluster id from condor_submit's output
    unless( open( SUBMIT, "<$cmd_file.out" ) )
    {
	warn "error opening \"$cmd_file.out\": $!\n";
	return 0;
    }
    while( <SUBMIT> )
    {
	if( /\d+ job\(s\) submitted to cluster (\d+)./ )
	{
	    $cluster = $1;
	    last;
	}
    }
    close SUBMIT;

    # if for some reason we didn't find the cluster id 
    unless( $cluster )
    {
	warn "error: couldn't find cluster id in condor_submit output\n";
	return 0;
    }
    
    # snarf info from the submit file and save for future reference
    unless( ParseSubmitFile( $cmd_file ) )
    {
	print "error: couldn't correctly parse submit file $cmd_file\n";
	return 0;
    }

    return $cluster;
}

# submits job/s to condor dagman
#
# - takes the name of a dag file as an argument
# - returns 0 upon failure, or cluster id upon success
sub TestSubmitDagman
{
    my $cmd_file = shift || croak "missing Dag file argument";
	my $cmd_args = shift || croak "missing Dagman Args";

    # reset global state
    # Reset();
	# Reset done in CondorTest.pm prior to submit

    # submit the file
    runCommand( "$CONDOR_SUBMIT_DAG $cmd_args -f -notification never $cmd_file > $cmd_file.out 2>&1" )
	|| return 0;

	$submit_time = time; # time reference for time callback

    # snarf the cluster id from condor_submit's output
    unless( open( SUBMIT, "<$cmd_file.out" ) )
    {
	warn "error opening \"$cmd_file.out\": $!\n";
	return 0;
    }
    while( <SUBMIT> )
    {
	if( /\d+ job\(s\) submitted to cluster (\d+)./ )
	{
	    $cluster = $1;
		debug("Cluster is $cluster\n");
	    last;
	}
    }
    close SUBMIT;

    # if for some reason we didn't find the cluster id 
    unless( $cluster )
    {
	warn "error: couldn't find cluster id in condor_submit output\n";
	return 0;
    }
    
    # snarf info from the submit file and save for future reference
	my $dagsubmitfile = $cmd_file  . ".condor.sub";
    unless( ParseSubmitFile( $dagsubmitfile ) )
    {
	print "error: couldn't correctly parse submit file $dagsubmitfile\n";
	return 0;
    }

    return $cluster;
}

sub Vacate
{
    my $machine = shift || croak "missing machine argument";
    return runCommand( "$CONDOR_VACATE $machine" );
}

sub Reschedule
{
    my $machine = shift;
    return runCommand( "$CONDOR_RESCHD $machine" );
}

# runs a command string, returning 0 if anything went wrong or 1 upon success
sub runCommand
{
    my $command_string = shift;
    my ($command) = split /\s+/, $command_string;
    
    debug( "running $command_string\n" );
    $retval = system( $command_string );

    # did command die abormally?
	
    if( ! safe_WIFEXITED( $retval ) )
    {
        print "error: $command died abnormally\n";
		#call error callback
		$info{'ErrorMessage'} = "error: $command died abnormally";
		if (defined $WantErrorCallback )
		{
			#debug("Calling error callback!!!!!! at @ $WantErrorCallback\n");
			&$WantErrorCallback( %info )
		}
        return 0;
    }

    # did command exit w/an error?
    if( safe_WEXITSTATUS( $retval ) != 0 )
    {
	$exitval = safe_WEXITSTATUS( $retval );
        print "error: $command failed (returned $exitval)\n";
		#call error callback
		$info{'ErrorMessage'} = "error: $command failed (returned $exitval)";
		if (defined $WantErrorCallback )
		{
			#debug("Calling error callback!!!!!! at @ $WantErrorCallback\n");
			&$WantErrorCallback( %info )
		}
        return 0;
    }

    return 1;
}

sub RegisterSubmit
{
    my $sub = shift || croak "missing argument";
    $SubmitCallback = $sub;
}

sub RegisterExecute
{
    my $sub = shift || croak "missing argument";
    $ExecuteCallback = $sub;
}

sub RegisterEvicted
{
    my $sub = shift || croak "missing argument";
    $EvictedCallback = $sub;
}

sub RegisterEvictedWithCheckpoint
{
    my $sub = shift || croak "missing argument";
    $EvictedWithCheckpointCallback = $sub;
}

sub RegisterEvictedWithRequeue
{
    my $sub = shift || croak "missing argument";
    $EvictedWithRequeueCallback = $sub;
}

sub RegisterEvictedWithoutCheckpoint
{
    my $sub = shift || croak "missing argument";
    $EvictedWithoutCheckpointCallback = $sub;
}

sub RegisterExit
{
    my $sub = shift || croak "missing argument";
    $ExitedCallback = $sub;
}

sub RegisterExitSuccess
{
    my $sub = shift || croak "missing argument";
    $ExitedSuccessCallback = $sub;
}    

sub RegisterExitFailure
{
    my $sub = shift || croak "missing argument";
    $ExitedFailureCallback = $sub;
}    

sub RegisterExitAbnormal
{
    my $sub = shift || croak "missing argument";
    $ExitedAbnormalCallback = $sub;
}
sub RegisterAbort
{
    my $sub = shift || croak "missing argument";
    $AbortCallback = $sub;
}
sub RegisterShadow
{
    my $sub = shift || croak "missing argument";
    $ShadowCallback = $sub;
}
sub RegisterHold
{
    my $sub = shift || croak "missing argument";
    $HoldCallback = $sub;
}
sub RegisterRelease
{
    my $sub = shift || croak "missing argument";
    $ReleaseCallback = $sub;
}
sub RegisterJobErr
{
    my $sub = shift || croak "missing argument";
    $JobErrCallback = $sub;
}

sub RegisterWantError
{
    my $sub = shift || croak "missing argument";
    $WantErrorCallback = $sub;
}

sub RegisterTimed
{
    my $sub = shift || croak "missing callback argument";
    my $delta = shift || croak "missing time argument";

    $TimedCallback = $sub;
	$TimedCallbackWait = $delta;
}

sub Wait
{
    while( wait() != -1 ) { };
    return $?;
}

# spawn process to monitor the submit log file and execute callbacks
# upon seeing registered events
sub Monitor
{
    my $timeout = shift;
    my $linenum = 0;
    my $line;
    my %info;
	my $timestamp = 0;

    debug( "Entering Monitor\n" );

# commented out until we can do it right -- pfc

#    # fork new process
#    $pid = fork();
#    die "error: fork() failed: $!\n" unless defined $pid;
#    # if we're the parent, return
#    return if $pid;
 
#    debug( "In Monitor child\n" );

	my $reallog = "";

    # open submit log
	if( exists $submit_info{'initialdir'} )
	{
		$reallog = $submit_info{'initialdir'} . "/" .  $submit_info{'log'};
    	unless( open( SUBMIT_LOG, "<$reallog" ) )
    	{
			warn "error opening $reallog: $!\n";
			print "Trying to open $reallog!!!!! Initialdir ...NOT... ignored!\n";
			return 0;
    	}
	}
	else
	{
    	unless( open( SUBMIT_LOG, "<$submit_info{'log'}" ) )
    	{
			warn "error opening $submit_info{'log'}: $!\n";
			print "Trying to open $submit_info{'log'}!!!!! Initialdir ignored!\n";
			return 0;
    	}
	}

  LINE:
    while( 1 )
    {
		#print "Now $num_active_jobs is number of active jobs\n";
	if( $saw_submit && $num_active_jobs == 0 )
	{
#	    debug( "num_active_jobs = $num_active_jobs -- monitor exiting\n" );
#	    exit 0;
	    debug( "num_active_jobs = $num_active_jobs -- monitor returning\n" );
	    return 1;
	}

	# once every event or second see if we should do a timedCallback
	%info = %submit_info; # get initial value of submit stuff in case no events come and we time out

	if(defined $TimedCallback)
	{
		CheckTimedCallback();
	}

	# read line from log (if we're at EOF, wait and re-try)
	$line = <SUBMIT_LOG>;
	while( ! defined $line )
	{
		sleep 2;
		if(defined $TimedCallback)
		{
			CheckTimedCallback();
		}
		$line = <SUBMIT_LOG>;
	}
	chomp $line;
	$linenum++;

      PARSE:

	# reset %info hash to clear details added for last event
	%info = %submit_info;

	# if this line is for another cluster, ignore
	if ( $line =~ /^\d+\s+\(0*(\d+)\./ && $1 != $cluster )
	{
#	    debug( "log line for cluster $1, not $cluster -- ignoring...\n" );
	    next LINE;
	}
	
	# 004: job evicted
	if( $line =~ /^004\s+\(0*(\d+)\.0*(\d+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw job evicted\n" );

	    # execute callback if one is registered
	    &$EvictedCallback( %info )
		if defined $EvictedCallback;

	    # read next line to see if job was checkpointed (but first
	    # sleep for 5 seconds to give it a chance to appear)
	    # [this should really loop like above, not sleep like this...]
	    # sleep 5;
	    $line = <SUBMIT_LOG>;
		while( ! defined $line )
		{
			sleep 2;
			if(defined $TimedCallback)
			{
				CheckTimedCallback();
			}
			$line = <SUBMIT_LOG>;
		}
	    chomp $line;
	    $linenum++;

	    if( $line =~ /^\s+\(0\) Job was not checkpointed\./ )
	    {
		debug( "job was evicted without ckpt\n" );
		# execute callback if one is registered
		&$EvictedWithoutCheckpointCallback( %info )
		    if defined $EvictedWithoutCheckpointCallback;

	    }
	    elsif( $line =~ /^\s+\(1\) Job was checkpointed\./ )
	    {
		debug( "job was evicted with ckpt\n" );
		# execute callback if one is registered
		&$EvictedWithCheckpointCallback( %info )
		    if defined $EvictedWithCheckpointCallback;
	    }
	    elsif( $line =~ /^\s+\(0\) Job terminated and was requeued.*$/ )
	    {
		debug( "job was evicted and requeued\n" );
		# execute callback if one is registered
		&$EvictedWithRequeueCallback( %info )
		    if defined $EvictedWithRequeueCallback;
	    }
	    else
	    {
		debug( "parse error on line $linenum of $info{'log'}:\n" .
		       "   no checkpoint message found after eviction: " .
		       "continuing...\n" );
		# re-parse line so we don't miss whatever it said
		goto PARSE;
	    }
	    next LINE;
	}

	# 005: job terminated
	if( $line =~ /^005\s+\(0*(\d+)\.0*(\d+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw job terminated\n" );

	    # decrement # of queued jobs so we will know when to exit monitor
	    $num_active_jobs--;

	    # execute callback if one is registered
	    &$ExitedCallback( %info )
		if defined $ExitedCallback;

	    # read next line to see how job terminated
	    $line = <SUBMIT_LOG>;
		while( ! defined $line )
		{
			sleep 2;
			if(defined $TimedCallback)
			{
				CheckTimedCallback();
			}
			$line = <SUBMIT_LOG>;
		}
	    chomp $line;
	    $linenum++;

	    # terminated successfully
	    if( $line =~ /^\s+\(1\) Normal termination \(return value 0\)/ )
	    {
		# execute callback if one is registered
		&$ExitedSuccessCallback( %info )
		    if defined $ExitedSuccessCallback;
	    }
	    # terminated w/error
	    elsif( $line =~ 
		   /^\s+\(1\) Normal termination \(return value (\d+)\)/ )
	    {
		$info{'retval'} = $1;
		# execute callback if one is registered
		&$ExitedFailureCallback( %info )
		    if defined $ExitedFailureCallback;
	    }
	    # abnormal termination
	    elsif( $line =~ /^\s+\(0\) Abnormal termination \(signal (\d+)\)/ )
	    {
		debug( "Loading $1 as info{'signal'}\n" );
		$info{'signal'} = $1;
		print "keys:".join(" ",keys %info)."\n";

		debug( "checking for core file...\n" );

		# read next line to find core file
		$line = <SUBMIT_LOG>;
		while( ! defined $line )
		{
			sleep 2;
			if(defined $TimedCallback)
			{
				CheckTimedCallback();
			}
			$line = <SUBMIT_LOG>;
		}
		chomp $line;
		$linenum++;

		if( $line =~ /^\s+\(1\) Corefile in: (.*)/ )
		{
		    $info{'core'} = $1;
		}
		elsif( $line =~ /^\s+\(0\) No core file/ )
		{
		    debug( "no core file found\n" );
		    # not sure what to do here with $info{'core'}...
		}
		else
		{
		    die( "parse error on line $linenum of $info{'log'}:\n" .
			   "   no core file message found after " .
			   "abornal termination: continuing...\n" );
		}
		# execute callback if one is registered
		&$ExitedAbnormalCallback( %info )
		    if defined $ExitedAbnormalCallback;
	    }
	    else
	    {
		debug( "parse error on line $linenum of $info{'log'}:\n" .
		       "   no termination status message found after " .
		       "termination: continuing...\n" );
		# re-parse line so we don't miss whatever it said
		goto PARSE;
	    }
	    next LINE;
	}

	# 007: shadow exception
	if( $line =~ /^007\s+\(0*(\d+)\.0*(\d+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw Shadow Exception\n" );

	    # decrement # of queued jobs so we will know when to exit monitor
	    #$num_active_jobs--;

	    # read next line to see how job terminated
	    $line = <SUBMIT_LOG>;
		while( ! defined $line )
		{
			sleep 2;
			if(defined $TimedCallback)
			{
				CheckTimedCallback();
			}
			$line = <SUBMIT_LOG>;
		}
	    chomp $line;
	    $linenum++;

		$info{'shadowerror'} = $line;

		# execute callback if one is registered
		&$ShadowCallback( %info )
		    if defined $ShadowCallback;

	    next LINE;
	}

	# 001: job executing
	elsif( $line =~ 
	       /^001\s+\(0*(\d+)\.0*(\d+).*<(\d+\.\d+\.\d+\.\d+):(\d+)>/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;
	    $info{'host'} = $3;
	    $info{'sinful'} = "<$3:$4>";
	    
	    debug( "Saw job executing\n" );

	    # execute callback if one is registered
	    &$ExecuteCallback( %info )
		if defined $ExecuteCallback;

	    next LINE;
	}

	# 000: job submitted
	elsif( $line =~ 
	       /^000\s+\(0*(\d+)\.0*(\d+).*<(\d+\.\d+\.\d+\.\d+):(\d+)>/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;
	    $info{'host'} = $3;
	    $info{'sinful'} = "<$3:$4>";

	    debug( "Saw job submitted\n" );
	    $submit_info{'cluster'} = $1; # squirrel it away for TimedWait

	    # mark that we've seen a submit so we can start watching # of jobs
	    $saw_submit = 1;
	    # increment # of queued jobs so we will know when to exit monitor
	    $num_active_jobs++;

	    # execute callback if one is registered
	    &$SubmitCallback( %info )
		if defined $SubmitCallback;

	    next LINE;
	}
	# 009: job aborted by user
	elsif( $line =~ 
		/^009\s+\((\d+)\.(\d+).*$/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw job abort cluster $1 job $2\n" );

	    # decrement # of queued jobs so we will know when to exit monitor
	    $num_active_jobs--;
	    
	    # execute callback if one is registered
	    &$AbortCallback( %info )
		if defined $AbortCallback;
	}
	# 012: job was held
	elsif( $line =~ 
	       /^012\s+\(0*(\d+)\.0*(\d+).*/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;
	    $info{'host'} = $3;
	    $info{'sinful'} = "<$3:$4>";
	    
	    debug( "Saw job held\n" );

	    
	    # execute callback if one is registered
	    &$HoldCallback( %info )
		if defined $HoldCallback;
	}
	# 013: job was released
	elsif( $line =~ 
	       /^013\s+\(0*(\d+)\.0*(\d+).*/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;
	    $info{'host'} = $3;
	    $info{'sinful'} = "<$3:$4>";
	    
	    debug( "Saw job released\n" );

	    
	    # execute callback if one is registered
	    &$ReleaseCallback( %info )
		if defined $ReleaseCallback;
	}
#	002 (171.000.000) 06/15 14:49:45 (0) Job file not executable.
	# 002: job not executable
	elsif( $line =~ /^002/ )
	{
	    # decrement # of queued jobs so we will know when to exit monitor
	    $num_active_jobs--;

	    # execute callback if one is registered
	    &$JobErrCallback( %info )
		if defined $JobErrCallback;
	}
    }
    return 1;
}

sub CheckTimedCallback
{
	my $diff = 0;
	my $cluster = $info{"cluster"};
	my $timestamp = 0;

	$timestamp = time; #get current time
	$diff = $timestamp - $submit_time;
	if( $diff >= $TimedCallbackWait)
	{
		#call timed callback
		debug("Called timed callback!!!!!!-- $cluster --\n");
		&$TimedCallback( %info )
			if defined $TimedCallback;
	}
}

sub debug
{
    my $string = shift;
    print( "DEBUG ", timestamp(), ": $string" ) if $DEBUG;
}

sub ParseSubmitFile
{
    my $submit_file = shift || croak "missing submit file argument";
    my $line = 0;

    if( ! open( SUBMIT_FILE, $submit_file ) )
    {
	print "error opening \"$submit_file\": $!\n";
	return 0;
    }
    
    debug( "reading submit file...\n" );
    while( <SUBMIT_FILE> )
    {
	chomp;
	$line++;

	# skip comments & blank lines
	next if /^#/ || /^\s*$/;

	# if this line is a variable assignment...
	if( /^(\w+)\s*\=\s*(.*)$/ )
	{
	    $variable = lc $1;
	    $value = $2;

	    # if line ends with a continuation ('\')...
	    while( $value =~ /\\\s*$/ )
	    {
		# remove the continuation
		$value =~ s/\\\s*$//;

		# read the next line and append it
		<SUBMIT_FILE> || last;
		$value .= $_;
	    }

	    # compress whitespace and remove trailing newline for readability
	    $value =~ s/\s+/ /g;
	    chomp $value;

	
		# Do proper environment substitution
	    if( $value =~ /(.*)\$ENV\((.*)\)(.*)/ )
	    {
			my $envlookup = $ENV{$2};
	    	#debug( "Found $envlookup in environment \n");
			$value = $1.$envlookup.$3;
	    }

	    debug( "$variable = $value\n" );
	    
	    # save the variable/value pair
	    $submit_info{$variable} = $value;
	}
	else
	{
#	    debug( "line $line of $submit_file not a variable assignment... " .
#		   "skipping\n" );
	}
    }
    return 1;
}

sub ParseMachineAds
{
    my $machine = shift || croak "missing machine argument";
    my $line = 0;

	if( ! open(PULL, "condor_status -l $machine 2>&1 |") )
    {
		print "error getting Ads for \"$machine\": $!\n";
		return 0;
    }
    
    debug( "reading machine ads from $machine...\n" );
    while( <PULL> )
    {
	chomp;
#	debug("Raw AD is $_\n");
	$line++;

	# skip comments & blank lines
	next if /^#/ || /^\s*$/;

	# if this line is a variable assignment...
	if( /^(\w+)\s*\=\s*(.*)$/ )
	{
	    $variable = lc $1;
	    $value = $2;

	    # if line ends with a continuation ('\')...
	    while( $value =~ /\\\s*$/ )
	    {
		# remove the continuation
		$value =~ s/\\\s*$//;

		# read the next line and append it
		<PULL> || last;
		$value .= $_;
	    }

	    # compress whitespace and remove trailing newline for readability
	    $value =~ s/\s+/ /g;
	    chomp $value;

	
		# Do proper environment substitution
	    if( $value =~ /(.*)\$ENV\((.*)\)(.*)/ )
	    {
			my $envlookup = $ENV{$2};
	    	#debug( "Found $envlookup in environment \n");
			$value = $1.$envlookup.$3;
	    }

	    #debug( "$variable = $value\n" );
	    
	    # save the variable/value pair
	    $machine_ads{$variable} = $value;
	}
	else
	{
#	    debug( "line $line of $submit_file not a variable assignment... " .
#		   "skipping\n" );
	}
    }
    return 1;
}

sub FetchMachineAds
{
	return %machine_ads;
}

sub FetchMachineAdValue
{
	my $key = shift @_;
	if(exists $machine_ads{$key})
	{
		return $machine_ads{$key};
	}
	else
	{
		return undef;
	}
}

sub DebugOn
{
    $DEBUG = 1;
}

sub DebugOff
{
    $DEBUG = 0;
}

sub timestamp {
    return scalar localtime();
}

sub safe_WIFEXITED {

	my $status = shift;

	# our sneaky way to detect if WIFEXITED macro is defined.
	eval{ my $tmp = WIFEXITED(0); };
	if ( $@ ) {
		# no WIFEXITED, so AND the status with 127, which
		# in theory should tell us if we died with a signal or not.
		return !($status & 127);
	} else {
		return WIFEXITED($status);
	}
}

sub safe_WEXITSTATUS {

	my $status = shift;

	# our sneaky way to detect if WEXITSTATUS macro is defined.
	eval{ my $tmp = WEXITSTATUS(0); };
	if ( $@ ) {
		# no WEXITSTATUS, so just do what WEXITSTATUS normally
		# does, which is shift 8 (divide by 256)
		return $status >> 8;
	} else {
		return WEXITSTATUS($status);
	}
}
