# CondorTest.pm - a Perl module for automated testing of Condor
#
# 19??-???-?? originally written by Tom Stanis (?)
# 2000-Jun-02 total overhaul by pfc@cs.wisc.edu and wright@cs.wisc.edu

package CondorTest;

require 5.0;
use Carp;
use Condor;
use FileHandle;

BEGIN
{
    # disable command buffering so output is flushed immediately
    STDOUT->autoflush();
    STDERR->autoflush();

    $MAX_CHECKPOINTS = 2;
    $MAX_VACATES = 3;

    @skipped_output_lines = ( );
    @expected_output = ( );
    $checkpoints = 0;
    $vacates = 0;
    %test;
}

sub ResetTest
{
    $handle = shift || croak "missing handle argument";
    $test{$handle} = undef;
    $checkpoints = 0;
    $vacates = 0;
}

sub ForceVacateJob
{
    my %info = @_;

	print "ForceVacateJob called\n";
	foreach my $key  (sort keys %info)
	{
		print "Info holds $key\n";
	}
	print "After sort keys in info........\n";

    return 0 if ( $checkpoints >= $MAX_CHECKPOINTS ||
		  $vacates >= $MAX_VACATES );

    # let the job run for a few seconds and then send vacate signal
    sleep 5;
    Condor::VacateJob( "\"$info{'cluster'}\"" );
    $vacates++;
    return 1;
}

sub ForceVacate
{
    my %info = @_;

    return 0 if ( $checkpoints >= $MAX_CHECKPOINTS ||
		  $vacates >= $MAX_VACATES );

    # let the job run for a few seconds and then send vacate signal
    sleep 5;
    Condor::Vacate( "\"$info{'sinful'}\"" );
    $vacates++;
    return 1;
}

sub RegisterSubmit
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "submit: missing function reference argument";

    $test{$handle}{"RegisterSubmit"} = $function_ref;
}
sub RegisterExecute
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "execute: missing function reference argument";

    $test{$handle}{"RegisterExecute"} = $function_ref;
}
sub RegisterEvicted
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "evict: missing function reference argument";

    $test{$handle}{"RegisterEvicted"} = $function_ref;
}
sub RegisterEvictedWithCheckpoint
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterEvictedWithCheckpoint"} = $function_ref;
}
sub RegisterEvictedWithRequeue
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterEvictedWithRequeue"} = $function_ref;
}
sub RegisterEvictedWithoutCheckpoint
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterEvictedWithoutCheckpoint"} = $function_ref;
}
sub RegisterExited
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterExited"} = $function_ref;
}
sub RegisterExitedSuccess
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "exit success: missing function reference argument";

    $test{$handle}{"RegisterExitedSuccess"} = $function_ref;
}
sub RegisterExitedFailure
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterExitedFailure"} = $function_ref;
}
sub RegisterExitedAbnormal
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterExitedAbnormal"} = $function_ref;
}
sub RegisterAbort
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterAbort"} = $function_ref;
}
sub RegisterShadow
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterShadow"} = $function_ref;
}
sub RegisterWantError
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterWantError"} = $function_ref;
}
sub RegisterHold
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterHold"} = $function_ref;
}
sub RegisterRelease
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterRelease"} = $function_ref;
}
sub RegisterJobErr
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";

    $test{$handle}{"RegisterJobErr"} = $function_ref;
}

sub RegisterTimed
{
    my $handle = shift || croak "missing handle argument";
    my $function_ref = shift || croak "missing function reference argument";
	my $alarm = shift || croak "missing wait time argument";

    $test{$handle}{"RegisterTimed"} = $function_ref;
    $test{$handle}{"RegisterTimedWait"} = $alarm;
}

sub DefaultOutputTest
{
    my %info = @_;

    croak "default_output_test called but no \@expected_output defined"
	unless defined @expected_output;

    Condor::debug( "\$info{'output'} = $info{'output'}\n" );

    CompareText( $info{'output'}, \@expected_output, @skipped_output_lines )
	|| die "$handle: FAILURE (STDOUT doesn't match expected output)\n";

    IsFileEmpty( $info{'error'} ) || 
	die "$handle: FAILURE (STDERR contains data)\n";
}

sub RunTest
{
    $handle              = shift || croak "missing handle argument";
    my $submit_file      = shift || croak "missing submit file argument";
    my $wants_checkpoint = shift;

    my $status           = -1;

	# moved the reset to preserve callback registrations which includes
	# an error callback at submit time..... Had to change timing
	Condor::Reset();

    croak "too many arguments" if shift;

    # this is kludgey :: needed to happen sooner for an error message callback in runcommand
    $Condor::submit_info{'handle'} = $handle;

    # this is kludgey :: needed to happen sooner for an error message callback in runcommand
    #$Condor::submit_info{'handle'} = $handle;

    # if we want a checkpoint, register a function to force a vacate
    # and register a function to check to make sure it happens
	if( $wants_checkpoint )
	{
		Condor::RegisterExecute( \&ForceVacate );
		Condor::RegisterEvictedWithCheckpoint( sub { $checkpoints++ } );
	} else {
		if(defined $test{$handle}{"RegisterExecute"}) {
			Condor::RegisterExecute($test{$handle}{"RegisterExecute"});
		}
	}

	CheckRegistrations();

    # submit the job and get the cluster id
    $cluster = Condor::TestSubmit( $submit_file );
    
    # if condor_submit failed for some reason return an error
    return 0 if $cluster == 0;

    # monitor the cluster and return its exit status
    $retval = Condor::Monitor();

    die "$handle: FAILURE (job never checkpointed)\n"
	if $wants_checkpoint && $checkpoints < 1;

    return $retval;
}

sub RunDagTest
{
    $handle              = shift || croak "missing handle argument";
    my $submit_file      = shift || croak "missing submit file argument";
    my $wants_checkpoint = shift;
	my $dagman_args = 	shift || croak "missing dagman args";

    my $status           = -1;

    croak "too many arguments" if shift;

	# moved the reset to preserve callback registrations which includes
	# an error callback at submit time..... Had to change timing
	Condor::Reset();

    # this is kludgey :: needed to happen sooner for an error message callback in runcommand
    $Condor::submit_info{'handle'} = $handle;

    # if we want a checkpoint, register a function to force a vacate
    # and register a function to check to make sure it happens
	if( $wants_checkpoint )
	{
		Condor::RegisterExecute( \&ForceVacate );
		Condor::RegisterEvictedWithCheckpoint( sub { $checkpoints++ } );
	} else {
		if(defined $test{$handle}{"RegisterExecute"}) {
			Condor::RegisterExecute($test{$handle}{"RegisterExecute"});
		}
	}

	CheckRegistrations();

    # submit the job and get the cluster id
    $cluster = Condor::TestSubmitDagman( $submit_file, $dagman_args );
    
    # if condor_submit failed for some reason return an error
    return 0 if $cluster == 0;

    # monitor the cluster and return its exit status
    $retval = Condor::Monitor();

    die "$handle: FAILURE (job never checkpointed)\n"
	if $wants_checkpoint && $checkpoints < 1;

    return $retval;
}

sub CheckRegistrations
{
    # any handle-associated functions with the cluster
    # or else die with an unexpected event
    if( defined $test{$handle}{"RegisterExitedSuccess"} )
    {
        Condor::RegisterExitSuccess( $test{$handle}{"RegisterExitedSuccess"} );
    }
    else
    {
	Condor::RegisterExitSuccess( sub {
	    die "$handle: FAILURE (got unexpected successful termination)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterExitedFailure"} )
    {
	Condor::RegisterExitFailure( $test{$handle}{"RegisterExitedFailure"} );
    }
    else
    {
	Condor::RegisterExitFailure( sub {
	    my %info = @_;
	    die "$handle: FAILURE (returned $info{'retval'})\n";
	} );
    }

    if( defined $test{$handle}{"RegisterExitedAbnormal"} )
    {
	Condor::RegisterExitAbnormal( $test{$handle}{"RegisterExitedAbnormal"} );
    }
    else
    {
	Condor::RegisterExitAbnormal( sub {
	    my %info = @_;
	    die "$handle: FAILURE (got signal $info{'signal'})\n";
	} );
    }

    if( defined $test{$handle}{"RegisterShadow"} )
    {
	Condor::RegisterShadow( $test{$handle}{"RegisterShadow"} );
    }

    if( defined $test{$handle}{"RegisterWantError"} )
    {
	Condor::RegisterWantError( $test{$handle}{"RegisterWantError"} );
    }

    if( defined $test{$handle}{"RegisterAbort"} )
    {
	Condor::RegisterAbort( $test{$handle}{"RegisterAbort"} );
    }
    else
    {
	Condor::RegisterAbort( sub {
	    my %info = @_;
	    die "$handle: FAILURE (job aborted by user)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterHold"} )
    {
	Condor::RegisterHold( $test{$handle}{"RegisterHold"} );
    }
    else
    {
	Condor::RegisterHold( sub {
	    my %info = @_;
	    die "$handle: FAILURE (job held by user)\n";
	} );
    }

    if( defined $test{$handle}{"RegisterSubmit"} )
    {
	Condor::RegisterSubmit( $test{$handle}{"RegisterSubmit"} );
    }

    if( defined $test{$handle}{"RegisterRelease"} )
    {
	Condor::RegisterRelease( $test{$handle}{"RegisterRelease"} );
    }
    #else
    #{
	#Condor::RegisterRelease( sub {
	    #my %info = @_;
	    #die "$handle: FAILURE (job released by user)\n";
	#} );
    #}

    if( defined $test{$handle}{"RegisterJobErr"} )
    {
	Condor::RegisterJobErr( $test{$handle}{"RegisterJobErr"} );
    }
    else
    {
	Condor::RegisterJobErr( sub {
	    my %info = @_;
	    die "$handle: FAILURE (job error -- see $info{'log'})\n";
	} );
    }

    # if we wanted to know about requeues.....
    if( defined $test{$handle}{"RegisterEvictedWithRequeue"} )
    {
        Condor::RegisterEvictedWithRequeue( $test{$handle}{"RegisterEvictedWithRequeue"} );
    } 

    # if evicted, call condor_resched so job runs again quickly
    if( !defined $test{$handle}{"RegisterEvicted"} )
    {
        Condor::RegisterEvicted( sub { sleep 5; Condor::Reschedule } );
    } else {
	Condor::RegisterEvicted( $test{$handle}{"RegisterEvicted"} );
    }

    if( defined $test{$handle}{"RegisterTimed"} )
    {
		Condor::RegisterTimed( $test{$handle}{"RegisterTimed"} , $test{$handle}{"RegisterTimedWait"});
    }
}


sub CompareText
{
    my $file = shift || croak "missing file argument";
    my $aref = shift || croak "missing array reference argument";
    my @skiplines = @_;
    my $linenum = 0;

#	print "DEBUG opening file $file to compare to array of expected results\n";
    open( FILE, "<$file" ) || die "error opening $file: $!\n";
    
    while( <FILE> )
    {
	$line = $_;
	chomp $line;
	$linenum++;

#	print "DEBUG: linenum $linenum\n";
#	print "DEBUG: \$line: $line\n";
#	print "DEBUG: \$\$aref[0] = $$aref[0]\n";

#	print "DEBUG: skiplines = \"@skiplines\"\n";
#	print "DEBUG: grep returns ", grep( /^$linenum$/, @skiplines ), "\n";

	next if grep /^$linenum$/, @skiplines;

	$expectline = shift @$aref;
	if( ! defined $expectline )
	{
	    die "$file contains more text than expected\n";
	}
	chomp $expectline;

#	print "DEBUG: \$expectline: $expectline\n";

	# if they match, go on
	next if $expectline eq $line;

	# otherwise barf
	warn "$file line $linenum doesn't match expected output:\n";
	warn "actual: $line\n";
	warn "expect: $expectline\n";
	return 0;
    }

    # barf if we're still expecting text but the file has ended
    ($expectline = shift @$aref ) && 
        die "$file incomplete, expecting:\n$expectline\n";

    # barf if there are skiplines we haven't hit yet
    foreach $num ( @skiplines )
    {
	if( $num > $linenum )
	{
	    warn "skipline $num > # of lines in $file ($linenum)\n";
	    return 0;
	}
	croak "invalid skipline argument ($num)" if $num < 1;
    }
    
#    print "DEBUG: CompareText successful\n";
    return 1;
}

sub IsFileEmpty
{
    my $file = shift || croak "missing file argument";
    return -z $file;
}

sub verbose_system 
{
	my @args = @_;
	my $rc = 0xffff & system @args;

	printf "system(%s) returned %#04x: ", @args, $rc;

	if ($rc == 0) 
	{
		print "ran with normal exit\n";
		return $rc;
	}
	elsif ($rc == 0xff00) 
	{
		print "command failed: $!\n";
		return $rc;
	}
	elsif (($rc & 0xff) == 0) 
	{
		$rc >>= 8;
		print "ran with non-zero exit status $rc\n";
		return $rc;
	}
	else 
	{
		print "ran with ";
		if ($rc &   0x80) 
		{
			$rc &= ~0x80;
			print "coredump from ";
			return $rc;
		}
		print "signal $rc\n"
	}
}

1;
