package CustomMachineResource;

# use Exporter;
# our @ISA = qw( Exporter );
# our @EXPORT = qw( ... );

use CondorTest;

#
# This hash derives from cmr-squid-monitor and has to be changed when it does.
#
our %squidIncrements = (
	"SQUID0" => 5.0,
	"SQUID1" => 1.0,
	"SQUID2" => 9.0,
	"SQUID3" => 4.0
);


#
# Verify that the configuration resulted in what we expect.
#
sub TestSlotAndSQUIDsCount {
	my( $expectedCount ) = @_;

	my $ads = parseMachineAds( "Name", "AssignedSQUIDs" );

	my %SQUIDs;
	my $totalCount = 0;
	foreach my $ad (@{$ads}) {
		++$totalCount;
		my $assignedSQUIDs = $ad->{ "AssignedSQUIDs" };
		foreach my $SQUID (split( /[, ]+/, $assignedSQUIDs )) {
			if( $SQUID =~ /^(SQUID\d)$/ ) {
				$SQUIDs{ $SQUID } = 1;
			}
		}
	}

	if( $totalCount != $expectedCount ) {
		die( "Failure: Found ${totalCount} slots, was expecting $expectedCount.\n" );
	}

	if( scalar(keys %SQUIDs) != 4 ) {
		die( "Failure: Found " . scalar(keys %SQUIDs) . " SQUIDs, was expecting 4.\n" );
	}

	RegisterResult( 1, check_name => "slot-and-SQUIDs-counts",
					   test_name => $testName );
}


#
# Verify that the individual UptimeSQUIDsSeconds are being reported properly.
#
sub TestUptimeSQUIDsSeconds {
	my $directAds = parseDirectMachineAds( "AssignedSQUIDs", "UptimeSQUIDsSeconds" );

	my $multiplier = undef;
	foreach my $ad (@{$directAds}) {
		my $value = $ad->{ "UptimeSQUIDsSeconds" };
		if( $value eq "undefined" ) { next; }

		my $total = 0;
		my $assignedSQUIDs = $ad->{ "AssignedSQUIDs" };
		foreach my $SQUID (split( /[, ]+/, $assignedSQUIDs )) {
			my $increment = $squidIncrements{ $SQUID };

			if(! defined( $increment )) {
				die( "Failure: Assigned SQUID '" . $ad->{ "AssignedSQUIDs" } . "' invalid.\n" );
			}

			$total += $squidIncrements{ $SQUID };
		}

		# print( "UptimeSQIDsSeconds ${value} % totalIncrement ${total}\n" );
		if( $value % $total != 0 ) {
			die( "Failure: '${assignedSQUIDs}' has bad uptime '${value}'.\n" );
		}

		if(! defined( $multiplier )) {
			$multiplier = $value / $total;
			# print( "Inferring sample count = ${multiplier}.\n" );
		} else {
			if( $value / $total != $multiplier ) {
				die( "Failure: '${assignedSQUIDs}' has a different sample count (" . $value / $total . " != " . $multiplier . ").\n" );
			}
		}
	}

	RegisterResult( 1, check_name => "UptimeSQUIDsSeconds",
					   test_name => $testName );
}


#
# Testing callbacks.
#

my $abnormal = sub {
	my %info = @_;
	my $ID = $info{ 'cluster' } . "." . $info{ 'job' };
	TLOG( "Job $ID exited abnormally...\n" );

	die( "Error: Want to see only submit, execute and successful completion\n" );
};

my $aborted = sub {
	my %info = @_;
	my $ID = $info{ 'cluster' } . "." . $info{ 'job' };
	TLOG( "Job $ID aborted...\n" );

	die( "Error: Want to see only submit, execute and successful completion\n" );
};

my $held = sub
{
	my %info = @_;
	my $ID = $info{ 'cluster' } . "." . $info{ 'job' };
	TLOG( "Job $ID held...\n" );

	die( "Error: Want to see only submit, execute and successful completion\n" );
};

my %jobIDToSQUIDsMap;
my $executed = sub
{
	my %info = @_;
	my $ID = $info{ 'cluster' } . "." . $info{ 'job' };

	my $directAds = parseDirectMachineAds( "JobID", "AssignedSQUIDs" );
	foreach my $ad (@{$directAds}) {
		my $jobID = $ad->{ "JobID" };
		if(! defined($jobID)) { next; }
		if( $jobID ne $info{ 'cluster' } . "." . $info{ 'job' } ) { next; }

		$jobIDToSQUIDsMap{ $jobID } = $ad->{ "AssignedSQUIDs" };
		TLOG( "Job $ID started on " . $jobIDToSQUIDsMap{ $jobID } . "...\n" );
	}
};

my $submitted = sub {
	my %info = @_;
	my $ID = $info{ 'cluster' } . "." . $info{ 'job' };
	TLOG( "Job $ID submitted...\n" );
};

my $success = sub {
	my %info = @_;
	my $ID = $info{ 'cluster' } . "." . $info{ 'job' };
	TLOG( "Job $ID succeeded...\n" );
};

my $on_evictedwithoutcheckpoint = sub {
	my %info = @_;
	my $ID = $info{ 'cluster' } . "." . $info{ 'job' };
	TLOG( "Job $ID evicted without checkpoint...\n" );

	die( "Error: Want to see only submit, execute and successful completion\n" );
};

my $failure = sub {
	my %info = @_;
	my $ID = $info{ 'cluster' } . "." . $info{ 'job' };
	TLOG( "Job $ID failed...\n" );

	die( "Error: Want to see only submit, execute and successful completion\n" );
};

my %eventLogUsage;
my $usage = sub {
	my %info = @_;
	my $ID = $info{ 'cluster' } . "." . $info{ 'job' };

	my $inUsage = 0;
	my @lines = split( "\n", $info{ 'UsageLines' } );
	for( my $i = 0; $i < scalar(@lines); ++$i ) {
		my $line = $lines[$i];
		if( (! $inUsage) && $line =~ /Partitionable Resources :/ ) {
			$inUsage = 1;
			next;
		}
		if( $inUsage ) {
			my( $resource, $colon, $usage, $requested, $allocated ) =
				split( " ", $line );

			if( $resource =~ m/SQUIDs/i ) {
				TLOG( "Job ${ID} used $usage $resource...\n" );
				$eventLogUsage{ $ID } = $usage;
			}
		}
	}

	if(! defined( $eventLogUsage{ $ID } )) {
		die( "Unable to find SQUIDs usage in event log, aborting.\n" );
	}
};


#
# Test-specific utility function.
#
sub checkUsageValue {
	my( $increment, $value ) = @_;

	#
	# We submit 53-second jobs.  The goal is to make sure we get exactly
	# five sampling intervals: a 60-second job will occasionally get six
	# intervals and be reported as having taken 62 seconds; a 52-second
	# job could therefore likewise end up only getting four intervals.
	#
	# Of course, the various sleep() implementations only promise not to
	# wake you up early, so on a sufficiently-loaded system, we may still
	# have problems with jobs taking longer than they should.
	#
	# We therefore report that the usage value is OK if it's within 10%
	# of the expected value; this should catch any gross violations
	# resulting from unexpected interactions with other parts of the code.
	#
	# We also, however, write a warning to the test log if the value is
	# within 10% of the expected value but not one of the acceptable
	# imprecisions.  These include, as of this writing:
	#
	# * Reporting 5 samples worth of usage over a period 1 second too long.
	# * Reporting 5 samples worth of usage over a period 1 second too short.
	#
	# We do not check for reporting 6 samples worth of usage over a period
	# two seconds too long on the theory that this should be very rare for
	# a 53-second job.
	#
	# FIXME: Instead, we modify the job to report, along with the SQUID and
	# the usage value, what the startd reports (if D_TEST is set) as the
	# sample period.  We can compute from that (divide by the monitoring
	# period, round down, and multiply by the increment) what the reported
	# value should be, and check against that, instead.
	#

	my $STARTD_CRON_SQUIDs_MONITOR_PERIOD = 10;
	my $expectedValue = $increment / $STARTD_CRON_SQUIDs_MONITOR_PERIOD;
	my $sadnessValue = ($increment * 5) / 51;
	my $madnessValue = ($increment * 5) / 49;

	if( $value == $expectedValue || $value == $sadnessValue || $value == $madnessValue ) {
		return 1;
	} else {
		print(	"WARNING: ${value} was not one of the expected values " .
				"(${sadnessValue}, ${expectedValue}, ${madnessValue})\n" );

		if( (0.9 * $expectedValue) <= $value && $value <= (1.1 * $expectedValue) ) {
			return 1;
		} else {
			print( "Failure: ${value} more than 10% different than expected (${expectedValue}).\n" );
			return 0;
		}
	}
}


#
# Run a job.  Verify that:
# * the job ad in condor_q contains the correct SQUIDsUsage
# * the job ad in the history file contains the correct SQUIDsUsage
# * the user log contains the correct SQUIDsUsage
#
# Run abother job.  Use it to verify that .update.ad was correctly populated.
#

sub TestSQUIDsUsage {
	my( $testName ) = @_;

	Condor::RegisterUsage( $usage );
	CondorTest::RegisterExitedAbnormal( $testName, $abnormal );
	CondorTest::RegisterAbort( $testName, $aborted );
	CondorTest::RegisterHold( $testName, $held );
	CondorTest::RegisterExecute( $testName, $executed );
	CondorTest::RegisterSubmit( $testName, $submitted );
	CondorTest::RegisterExitedSuccess( $testName, $success );
	CondorTest::RegisterEvictedWithoutCheckpoint( $testName, $on_evictedwithoutcheckpoint );
	CondorTest::RegisterExitedFailure( $testName, $failure );

	my $submitFileName = "cmr-monitor-basic.cmd";
	if( CondorTest::RunTest( $testName, $submitFileName, 0 ) ) {

		# For each job, check its history file for SQUIDsUsage and see if that
		# value is sane, given what we know that the monitor is reporting for
		# each device.
		foreach my $jobID (keys %jobIDToSQUIDsMap) {
			my $SQUID = $jobIDToSQUIDsMap{ $jobID };
			my $increment = $squidIncrements{ $SQUID };
			if(! defined( $increment )) {
				die( "Failure: SQUID '${SQUID}' invalid.\n" );
			}

			my $jobAds = parseHistoryFile( $jobID, "SQUIDsUsage" );
			if( scalar( @{$jobAds} ) != 1 ) {
				die( "Did not get exactly one (" . scalar( @{$jobAds} ) . ") job ad for job ID '${jobID}', aborting.\n" );
			}

			foreach my $ad (@{$jobAds}) {
				my $value = $ad->{ "SQUIDsUsage" };

				# The event log's report is rounded for readability.
				my $elValue = sprintf( "%.2f", $value );
				if( $elValue != $eventLogUsage{ $jobID } ) {
					die( "Value in event log (" . $eventLogUsage{ $jobID } . ") does not equal value in history file (${value}), aborting.\n" );
				}

				if( checkUsageValue( $increment, $value ) ) {
					RegisterResult( 1, check_name => $SQUID . "-usage", test_name => $testName );
				} else {
					RegisterResult( 0, check_name => $SQUID . "-usage", test_name => $testName );
				}
			}
		}
	} else {
		die( "Error: $testName: CondorTest::RunTest(${submitFileName}) failed\n" );
	}

	my $clusterID;
	my $setClusterID = sub {
		my( $cID ) = @_;
		$clusterID = $cID;
	};

	# Poll the .update.ad file and then check its results.
	%jobIDToSQUIDsMap = ();
	$submitFileName = "cmr-monitor-basic-ad.cmd";
	if( CondorTest::RunTest( $testName, $submitFileName, 0, $setClusterID ) ) {
		my $lineCount = 0;
		my $outputFileBaseName = "cmr-monitor-basic-ad.${clusterID}.";
		for( my $i = 0; $i < 4; ++$i ) {
			my $outputFileName = $outputFileBaseName . $i . ".out";

			open( my $fh, '<', $outputFileName ) or
				die( "Error: ${testName}: could not open '${outputFileName}'\n" );

			while( my $line = <$fh> ) {
				++$lineCount;
				my( $SQUID, $value ) = split( ' ', $line );

				my $increment = $squidIncrements{ $SQUID };
				if(! defined( $increment )) {
					die( "Failure: SQUID '${SQUID}' invalid.\n" );
				}

				if( checkUsageValue( $increment, $value ) ) {
					RegisterResult( 1, check_name => $SQUID . "-update.ad-${lineCount}", test_name => $testName );
				} else {
					RegisterResult( 0, check_name => $SQUID . "-update.ad-${lineCount}", test_name => $testName );
				}
			}

			close( $fh );
		}

		# Each test job appends four lines to the log.
		if( $lineCount != 16 ) {
			die( "Error: $testName: 'cmr-monitor-basic-ad.out' had $lineCount lines, not 16.\n" );
		}
	} else {
		die( "Error: $testName: CondorTest::RunTest(${submitFileName}) failed\n" );
	}

	return 0;
}

# -----------------------------------------------------------------------------

sub parseAutoFormatLines {
	my @lines = @{$_[0]};
	my @attributes = @{$_[1]};

	my @ads;
	foreach my $line (@lines) {
		my %ad;
		my @values = split( ' ', $line );
		for( my $i = 0; $i < scalar(@values); ++$i ) {
			$ad{ $attributes[$i] } = $values[$i];
		}
		push( @ads, \%ad );
	}

	return \@ads;
}

sub parseMachineAds {
	my @attributes = @_;
	my $attributeList = join( " ", @attributes );

	my @lines = ();
	my $result = CondorTest::runCondorTool( "condor_status -af ${attributeList}", \@lines, 2, { emit_output => 0 } );
	return parseAutoFormatLines( \@lines, \@attributes );
}

sub parseDirectMachineAds {
	my @attributes = @_;
	my $attributeList = join( " ", @attributes );

	# It's super sad that this is the least-annoying way to do this.
	my $startdAddressFile = `condor_config_val STARTD_ADDRESS_FILE`;
	chomp( $startdAddressFile );
	my $startdAddress = `head -n 1 ${startdAddressFile}`;
	chomp( $startdAddress );

	my @lines = ();
	my $result = CondorTest::runCondorTool( "condor_status -direct '${startdAddress}' -af ${attributeList}", \@lines, 2, { emit_output => 0 } );
	return parseAutoFormatLines( \@lines, \@attributes );
}

sub parseHistoryFile {
	my $jobID = shift( @_ );
	my @attributes = @_;
	my $attributeList = join( " ", @attributes );

	my @lines = ();
	my $result = CondorTest::runCondorTool( "condor_history ${jobID} -af ${attributeList}", \@lines, 2, { emit_output => 0 } );
	return parseAutoFormatLines( \@lines, \@attributes );
}

1;
