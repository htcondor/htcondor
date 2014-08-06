##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************


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

use strict;
use warnings;

use Carp;
use File::Spec;
use POSIX qw/sys_wait_h strftime/;

use CondorUtils;

my $CONDOR_SUBMIT = 'condor_submit';
my $CONDOR_SUBMIT_DAG = 'condor_submit_dag';
my $CONDOR_VACATE = 'condor_vacate';
my $CONDOR_VACATE_JOB = 'condor_vacate_job';
my $CONDOR_RESCHD = 'condor_reschedule';
my $CONDOR_RM = 'condor_rm';

my $DEBUGLEVEL = 1; # turn on lowest level output
my $cluster = 0;
my $num_active_jobs = 0;
my $saw_submit = 0;
my %submit_info;
my %info; # assigned value of %submit_info frequently

my $submit_time = 0;
my $timer_time = 0;
my $TimedCallbackWait = 0;
my $SubmitCallback;
my $ExecuteCallback;
#my $EvictedCallback;
my $EvictedWithCheckpointCallback;
my $EvictedWithRequeueCallback;
my $EvictedWithoutCheckpointCallback;
my $ExitedCallback;
my $ExitedSuccessCallback;
my $ExitedFailureCallback;
my $ExitedAbnormalCallback;
my $AbortCallback;
my $SuspendedCallback;
my $UnsuspendedCallback;
my $ShadowCallback;
my $ImageUpdatedCallback;
my $HoldCallback;
my $ReleaseCallback;
my $DisconnectedCallback;
my $ReconnectedCallback;
my $ReconnectFailedCallback;
my $JobErrCallback;
my $TimedCallback;
my $WantErrorCallback;
my $ULogCallback;


sub Reset
{
    $cluster = 0;
    $num_active_jobs = 0;
    $saw_submit = 0;
    %submit_info = ();

	$submit_time = 0;
	$timer_time = 0;
	$TimedCallbackWait = 0;

    undef $SubmitCallback;
    undef $ExecuteCallback;
    #undef $EvictedCallback;
    undef $EvictedWithCheckpointCallback;
    undef $EvictedWithRequeueCallback;
    undef $EvictedWithoutCheckpointCallback;
    undef $ExitedCallback;
    undef $ExitedSuccessCallback;
    undef $ExitedFailureCallback;
    undef $ExitedAbnormalCallback;
    undef $AbortCallback;
    undef $ShadowCallback;
	undef $ImageUpdatedCallback;
    undef $HoldCallback;
    undef $ReleaseCallback;
    undef $DisconnectedCallback;
    undef $ReconnectedCallback;
    undef $ReconnectFailedCallback;
    undef $JobErrCallback;
    undef $TimedCallback;
    undef $WantErrorCallback;
    undef $ULogCallback;
}

sub SetHandle
{
	my $handle = shift;
	$submit_info{'handle'} = $handle;
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
		debug("Cluster is $cluster\n",5);
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
	my $submitOutput = "";

    my $debug_arg = "";

    #if( $DEBUG ) {
	$debug_arg = "-debug";
    #}

    # reset global state
    # Reset();
	# Reset done in CondorTest.pm prior to submit

    # submit the file
    runCommand( "$CONDOR_SUBMIT $debug_arg $cmd_file > $cmd_file.out 2>&1" )
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
	$submitOutput .= $_;
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
	warn "error: couldn't find cluster id in condor_submit output:\n $submitOutput";
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
	my $argisrefarray = ref $cmd_file;
	my $outfile = $cmd_file;
	my $cmdfiles = "";

	print "ref said type is <<$argisrefarray>>\n";
    # reset global state
    # Reset();
	# Reset done in CondorTest.pm prior to submit

    # submit the file
	if($argisrefarray eq "ARRAY") {
		$outfile = ${$cmd_file}[0];
		foreach my $ddd (@{$cmd_file}){
			$cmdfiles = $cmdfiles . " $ddd";
		}
		print "Multiple Dag Files: <$cmdfiles>\n";
    	runCommand( "$CONDOR_SUBMIT_DAG $cmd_args -f -notification never $cmdfiles > $outfile.out 2>&1" )
			|| return 0;
	} else {
    	runCommand( "$CONDOR_SUBMIT_DAG $cmd_args -f -notification never $cmd_file > $outfile.out 2>&1" )
			|| return 0;
	}

	$submit_time = time; # time reference for time callback

    # snarf the cluster id from condor_submit's output
    unless( open( SUBMIT, "<$outfile.out" ) )
    {
	warn "error opening \"$outfile.out\": $!\n";
	return 0;
    }
    while( <SUBMIT> )
    {
	if( /\d+ job\(s\) submitted to cluster (\d+)./ )
	{
	    $cluster = $1;
		debug("Cluster is $cluster\n",5);
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
	my $dagsubmitfile = $outfile  . ".condor.sub";
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
    return runCommand( "$CONDOR_VACATE $machine > condor_vacate.out 2>&1" );
}

sub Reschedule
{
    my $machine = shift;
	
	if(defined $machine) {
    	return runCommand( "$CONDOR_RESCHD $machine > condor_resched.out 2>&1" );
	} else {
    	return runCommand( "$CONDOR_RESCHD > condor_resched.out 2>&1" );
	}
}

# runs a command string, returning 0 if anything went wrong or 1 upon success
sub runCommand
{
    my $command_string = shift;
    my ($command) = split /\s+/, $command_string;
    
    debug( "running $command_string\n" ,5);
    my $retval = system( $command_string );

    # did command die abormally?
	
    if( ! safe_WIFEXITED( $retval ) )
    {
        print "error: $command died abnormally\n";
		#call error callback
		$info{'ErrorMessage'} = "error: $command died abnormally";
		if (defined $WantErrorCallback )
		{
			debug("Calling error callback!!!!!! at @ $WantErrorCallback\n",6);
			&$WantErrorCallback( %info );
			return 1;
		}
		DumpFailedCommand($command_string);
        return 0;
    }

    # did command exit w/an error?
    if( safe_WEXITSTATUS( $retval ) != 0 )
    {
		my $exitval = safe_WEXITSTATUS( $retval );
        print "error: $command failed (returned $exitval)\n";
		#call error callback
		$info{'ErrorMessage'} = "error: $command failed (returned $exitval)";
		if (defined $WantErrorCallback )
		{
			debug("Calling error callback!!!!!! at @ $WantErrorCallback\n",6);
			&$WantErrorCallback( %info );
			return 1;
		}
		DumpFailedCommand($command_string);
        return 0;
    }

    return 1;
}

sub DumpFailedCommand
{
    my $cmd = shift @_;
    my $output = "";
    print "ERROR: $cmd failed.\n";
	# was command one that saved error and output to a file?
    if($cmd=~ /(.*)\s+>\s+(.*)\s+2>&1/) {
        $output = $2;
		if(!open( MACH, "<$output" )) {
			warn "$cmd did not have an output file to add\n";
		} else {
    		print "ERROR: Output follows:\n";
    		while(<MACH>) {
        		print "ERROR: $_";
    		}
    		close(MACH);
    		print "ERROR: Output done:\n";
		}
    }
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

# only spcific evictions are enabled
#sub RegisterEvicted
#{
    #my $sub = shift || croak "missing argument";
    #$EvictedCallback = $sub;
#}

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
sub RegisterImageUpdated
{
    my $sub = shift || croak "missing argument";
    $ImageUpdatedCallback = $sub;
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
sub RegisterDisconnected
{
    my $sub = shift || croak "missing argument";
    $DisconnectedCallback = $sub;
	debug("Registering Disconnnected callback\n",2);
}
sub RegisterReconnected
{
    my $sub = shift || croak "missing argument";
    $ReconnectedCallback = $sub;
	debug("Registering Reconnnected callback\n",2);
}
sub RegisterReconnectFailed
{
    my $sub = shift || croak "missing argument";
    $ReconnectFailedCallback = $sub;
}
sub RegisterJobErr
{
    my $sub = shift || croak "missing argument";
    $JobErrCallback = $sub;
}
sub RegisterULog
{
    my $sub = shift || croak "missing argument";
    $ULogCallback = $sub;
}
sub RegisterWantError
{
    my $sub = shift || croak "missing argument";
    $WantErrorCallback = $sub;
}
sub RegisterSuspended
{
    my $sub = shift || croak "missing argument";
    $SuspendedCallback = $sub;

	debug("Registering Suspended callback\n",2);
}
sub RegisterUnsuspended
{
    my $sub = shift || croak "missing argument";
    $UnsuspendedCallback = $sub;

	debug("Registering Unsuspended callback\n",2);
}

sub RegisterTimed
{
    my $sub = shift || croak "missing callback argument";
    my $delta = shift || croak "missing time argument";

	$timer_time = time;
    $TimedCallback = $sub;
	$TimedCallbackWait = $delta;
}

sub RemoveTimed
{
    $timer_time = undef;
    $TimedCallback = undef;
    $TimedCallbackWait = undef;
}

sub Wait
{
    while( wait() != -1 ) { };
    return $?;
}

sub IsAbsolutePath {
    my ($path) = @_;
    return File::Spec->file_name_is_absolute($path);
}

# spawn process to monitor the submit log file and execute callbacks
# upon seeing registered events
# July 26, 2013
#
# Major architecture change as we used more callbacks for testing
# I stumbled on value in the test which were to be changed by the callbacks were not being
# changed. What was happening in DoTest was that we would fork and the child was
# running monitor and that callbacks were changing the variables in the child's copy.
# The process that is the test calling DoTest was simply waiting for the child to die
# so it could clean up. Now The test switches to be doing the monitoring and the 
# callbacks switch it back up to the test code for a bit. The monitor and the 
# test had always been lock-steped anyways so getting rid of the child
# has had little change except that call backs can be fully functional now.
sub Monitor
{
    my $timeout = shift;
    my $linenum = 0;
    my $line;
    #my %info;
	my $timestamp = 0;

    debug( "Entering Monitor\n" ,5);

# commented out until we can do it right -- pfc

#    # fork new process
#    $pid = fork();
#    die "error: fork() failed: $!\n" unless defined $pid;
#    # if we're the parent, return
#    return if $pid;
 
    debug( "In Monitor child\n" ,6);

	my $reallog = "";

    # open submit log
	if( exists $submit_info{'initialdir'} )
	{
		my $logrequest = $submit_info{'log'};
		if( IsAbsolutePath($logrequest) )
		{
			#print "SLASH RELATIVE Log file, LEAVE IT ALONE!\n";
			unless( open( SUBMIT_LOG, "<$submit_info{'log'}" ) )
			{
				warn "error opening $submit_info{'log'}: $!\n";
				print "Trying to open $submit_info{'log'}!!!!! Initialdir ignored!\n";
				return 0;
			}
		}
		else
		{
			$reallog = $submit_info{'initialdir'} . "/" .  $submit_info{'log'};
			unless( open( SUBMIT_LOG, "<$reallog" ) )
			{
				warn "error opening $reallog: $!\n";
				print "Trying to open $reallog!!!!! Initialdir ...NOT... ignored!\n";
				return 0;
			}
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
	    debug( "num_active_jobs = $num_active_jobs -- monitor returning\n" ,5);
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
	CondorUtils::fullchomp($line);
	$linenum++;

      PARSE:

	# reset %info hash to clear details added for last event
	%info = %submit_info;

	# if this line is for another cluster, ignore
	if ( $line =~ /^\d+\s+\(0*(\d+)\./ && $1 != $cluster )
	{
	    #debug( "log line for cluster $1, not $cluster -- ignoring...\n" ,1);
	    next LINE;
	}
	
	# 004: job evicted
	if( $line =~ /^004\s+\(0*(\d+)\.0*(\d+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw job ($1.$2) evicted\n" ,2);

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
	    CondorUtils::fullchomp($line);
	    $linenum++;

	    if( $line =~ /^\s+\(0\) Job was not checkpointed\./ )
	    {
		debug( "job was evicted without ckpt\n" ,2);
		# execute callback if one is registered
		&$EvictedWithoutCheckpointCallback( %info )
		    if defined $EvictedWithoutCheckpointCallback;

	    }
	    elsif( $line =~ /^\s+\(1\) Job was checkpointed\./ )
	    {
		debug( "job was evicted with ckpt\n" ,2);
		# execute callback if one is registered
		&$EvictedWithCheckpointCallback( %info )
		    if defined $EvictedWithCheckpointCallback;
	    }
	    elsif( $line =~ /^\s+\(0\) Job terminated and was requeued.*$/ )
	    {
		debug( "job was evicted and requeued\n" ,2);
		# execute callback if one is registered
		&$EvictedWithRequeueCallback( %info )
		    if defined $EvictedWithRequeueCallback;
	    }
	    else
	    {
		debug( "parse error on line $linenum of $info{'log'}:\n" .
		       "   no checkpoint message found after eviction: " .
		       "continuing...\n" ,1);
		debug( "Eviction type expected:<$line>\n", 1);
		# re-parse line so we don't miss whatever it said
		goto PARSE;
	    }
		# let all the special callbacks got first
	    # execute callback if one is registered
		# Are there any other types of evictions?
	    #&$EvictedCallback( %info )
		#if defined $EvictedCallback;

	    next LINE;
	}

	# 005: job terminated
	if( $line =~ /^005\s+\(0*(\d+)\.0*(\d+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw job terminated\n" ,2);

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
	    CondorUtils::fullchomp($line);
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
		debug( "Loading $1 as info{'signal'}\n" ,1);
		$info{'signal'} = $1;
		#print "keys:".join(" ",keys %info)."\n";

		debug( "checking for core file...\n" ,1);

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
	    CondorUtils::fullchomp($line);
		$linenum++;

		if( $line =~ /^\s+\(1\) Corefile in: (.*)/ )
		{
		    $info{'core'} = $1;
		}
		elsif( $line =~ /^\s+\(0\) No core file/ )
		{
		    debug( "no core file found\n" ,5);
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
		       "termination: continuing...\n" ,2);
		# re-parse line so we don't miss whatever it said
		goto PARSE;
	    }
	    next LINE;
	}

	# 006: Image Size Updated
	if( $line =~ /^006\s+\(0*(\d+)\.0*(\d+).*?Image\s+size\s+of\s+job\s+updated:\s+(\d+)\s*$/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;
		$info{'imagesize'} = $3;

	    debug( "Saw Image Size Update <$3>\n" ,2);

	    # read next line to see current Megs
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
	    CondorUtils::fullchomp($line);
	    $linenum++;

		# Grab current memory in Megs
		if($line =~ /^\s*(\d+).*$/) {
			$info{'megs'} = $1;
			debug( "Memory usage $1 (MB)\n",2);
		}

	    # read next line to see current RSS
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
	    CondorUtils::fullchomp($line);
	    $linenum++;

		# Grab current memory in Megs
		if($line =~ /^\s*(\d+).*$/) {
			$info{'rss'} = $1;
			debug( "RSS usage $1 (K)\n",2);
		}

		# execute callback if one is registered
		    if (defined $ImageUpdatedCallback) {
				&$ImageUpdatedCallback( %info );
			} else {
				print "Saw Image Size Update <$info{'imagesize'}>\n";
			}

	    next LINE;

	}

	# 007: shadow exception
	if( $line =~ /^007\s+\(0*(\d+)\.0*(\d+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw Shadow Exception\n" ,2);

		if(! defined $ShadowCallback)
		{
    		runCommand( "$CONDOR_RM $1 > condor_rm.out 2>&1" );
			die "Unexpected Shadow Exception. Job Removed!!\n";
		}

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
	    CondorUtils::fullchomp($line);
	    $linenum++;

		$info{'shadowerror'} = $line;

		# execute callback if one is registered
		&$ShadowCallback( %info )
		    if defined $ShadowCallback;

	    next LINE;
	}

	# 022: Job Disconnected
	if( $line =~ /^022\s+\(0*(\d+)\.0*(\d+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw Job Disconnected\n" ,1);

	    # read next line to see cause
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
	    CondorUtils::fullchomp($line);
	    $linenum++;

		$info{'cause'} = $line;

	    # read next line to see recovery
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
	    CondorUtils::fullchomp($line);
	    $linenum++;

		$info{'recovery'} = $line;

		# execute callback if one is registered
		&$DisconnectedCallback( %info )
		    if defined $DisconnectedCallback;

	    next LINE;
	}

	# 023: Job Reconnected
	if( $line =~ /^023\s+\(0*(\d+)\.0*(\d+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw Job Reconnected\n" ,1);

	    # read next line to see cause
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
	    CondorUtils::fullchomp($line);
	    $linenum++;

		$info{'startd'} = $line;

	    # read next line to see recovery
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
	    CondorUtils::fullchomp($line);
	    $linenum++;

		$info{'starter'} = $line;

		# execute callback if one is registered
		&$ReconnectedCallback( %info )
		    if defined $ReconnectedCallback;

	    next LINE;
	}

	# 024: Job Reconnect Failed
	if( $line =~ /^024\s+\(0*(\d+)\.0*(\d+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw Job Reconnect Fail\n" ,2);
		print "Saw reconnect fail\n";

	    # read next line to see cause
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
	    CondorUtils::fullchomp($line);
	    $linenum++;

		$info{'cause'} = $line;

	    # read next line to see recovery
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
	    CondorUtils::fullchomp($line);
	    $linenum++;

		$info{'recovery'} = $line;

		# execute callback if one is registered
		&$ReconnectFailedCallback( %info )
		    if defined $ReconnectFailedCallback;

	    next LINE;
	}

	# 010: Job Suspended
	if( $line =~ /^010\s+\(0*(\d+)\.0*(\d+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw Job Suspended\n" ,1);

	    # read next line to see processes affected
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
	    CondorUtils::fullchomp($line);
	    $linenum++;

		$info{'processes'} = $line;

		# execute callback if one is registered
		&$SuspendedCallback( %info )
		    if defined $SuspendedCallback;

	    next LINE;
	}

	# 010: Job Unsuspended
	if( $line =~ /^011\s+\(0*(\d+)\.0*(\d+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;

	    debug( "Saw Job Unsuspended\n" ,1);

		# execute callback if one is registered
		&$UnsuspendedCallback( %info )
		    if defined $UnsuspendedCallback;

	    next LINE;
	}

	# 001: job executing
	elsif( $line =~ 
	       /^001\s+\(0*(\d+)\.0*(\d+).*: (.+)/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;
	    $info{'sinful'} = $3;
	    $info{'host'} = $3;
	    # Strip off the < and > from a sinful string for 'host'
	    # For grid universe jobs, there won't be any
	    $info{'host'} =~ s/^<([^>]+)>/$1/;
	    
	    debug( "Saw job executing\n" ,2);

	    # execute callback if one is registered
	    &$ExecuteCallback( %info )
		if defined $ExecuteCallback;

	    next LINE;
	}

	# 000: job submitted
	elsif( $line =~ 
	       /^000\s+\(0*(\d+)\.0*(\d+).*<([^>]+)>/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;
	    $info{'host'} = $3;
	    $info{'sinful'} = "<$3>";

	    debug( "Saw job submitted\n" ,2);
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

	    debug( "Saw job abort cluster $1 job $2\n" ,2);

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
	    #$info{'host'} = $3;
	    #$info{'sinful'} = "<$3:$4>";
	    
	    # read next line to see how job was held
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
	    CondorUtils::fullchomp($line);
	    $linenum++;

		$info{'holdreason'} = $line;

	    debug( "Saw job held\n" ,1);

	    
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
	    #$info{'host'} = $3;
	    #$info{'sinful'} = "<$3:$4>";
	    
	    debug( "Saw job released\n" ,1);

	    
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
#	008 (171.000.000) 06/15 14:49:45 user defined text
	# 008: chirp ulog
	elsif( $line =~ 
		/^008\s+\((\d+)\.(\d+)\.\d+\)\s+([0-9\/]+)\s+([0-9:]+)\s*(.*)$/ )
	{
	    $info{'cluster'} = $1;
	    $info{'job'} = $2;
	    $info{'date'} = $3;
	    $info{'time'} = $4;
	    $info{'ulog'} = $5;

	    debug( "Saw job ulog cluster $1 job $2 : $5\n" ,1);

	    &$ULogCallback( %info )
		if defined $ULogCallback;
	}
    }
    return 1;
}

sub CheckTimedCallback
{
	my $diff = 0;
	my $cluster = $submit_info{"cluster"};
	my $timestamp = 0;

	$timestamp = time; #get current time
	$diff = $timestamp - $timer_time;
	debug("Delta for timer is $diff\n",6);
	if( $diff >= $TimedCallbackWait)
	{
		#call timed callback
		debug("Called timed callback!!!!!!-- $cluster --\n",5);
		if(defined $TimedCallback) {
			my $tempcallback = $TimedCallback; # save and removed callback request
			RemoveTimed();
			&$tempcallback(%info);
		}
	}
}

################################################################################
#
# debug( string, level )
#
# If a level is passed in then the printing varies by current level
# otherwise we react only to dubug on or off at the momment.
#
# Predefined levels:
#		5  Condor.pm
#		4  CondorTest.pm
#		3  CondorPersonal.pm
#		2  batch_test.pl
#		1  all existing prints being converted
#
################################################################################

sub debug {
    my ($msg, $level) = @_;
    
    if(!(defined $level)) {
    	print timestamp() . ": $msg";
    }
    elsif($level <= $DEBUGLEVEL) {
    	print timestamp() . ": $msg";
    }
}

sub DebugLevel {
	my $newlevel = shift;
	my $oldlevel = $DEBUGLEVEL;
    $DEBUGLEVEL = $newlevel;
	return($oldlevel);
}

sub ParseSubmitFile
{
    my $submit_file = shift || croak "missing submit file argument";
    my $line = 0;
	my $variable;
	my $value;

    if( ! open( SUBMIT_FILE, $submit_file ) )
    {
	print "error opening \"$submit_file\": $!\n";
	return 0;
    }
    
    debug( "reading submit file...\n" ,5);
    while( <SUBMIT_FILE> )
    {
	CondorUtils::fullchomp($_);	# do a windows strip, fine for linux
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
		# Don't change white space. Some environment tests expect tabs to stay tabs.
	    # $value =~ s/\s+/ /g;
	    CondorUtils::fullchomp($value);

	
		# Do proper environment substitution
	    if( $value =~ /(.*)\$ENV\((.*)\)(.*)/ )
	    {
			my $envlookup = $ENV{$2};
	    	debug( "Found $envlookup in environment \n",6);
			$value = $1.$envlookup.$3;
	    }

	    debug( "$variable = $value\n" ,5);
	    
	    # save the variable/value pair
	    $submit_info{$variable} = $value;
	}
	else
	{
	    debug( "line $line of $submit_file not a variable assignment... " .
		   "skipping\n" ,6);
	}
    }
    return 1;
}

sub timestamp {
    return strftime("%H:%M:%S", localtime);
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


1;
