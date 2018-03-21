package CustomMachineResource;

# use Exporter;
# our @ISA = qw( Exporter );
# our @EXPORT = qw( ... );

use strict;
use warnings;

use CondorTest;

#
# This hash derives from cmr-squid-monitor and has to be changed when it does.
#
my %squidIncrements = (
	"SQUID0" => 5.0,
	"SQUID1" => 1.0,
	"SQUID2" => 9.0,
	"SQUID3" => 4.0
);

#
# This hash derives from cmr-tako-monitor and has to be changed when it does.
#
my %takoIncrements = (
	"TAKO0"	=> 500,
	"TAKO1"	=> 100,
	"TAKO2"	=> 900,
	"TAKO3"	=> 400
);

my %increments = (
	"SQUID"	=> \%squidIncrements,
	"TAKO" 	=> \%takoIncrements
);


#
# Verify that the configuration resulted in what we expect.
#
sub TestSlotAndSQUIDsCount {
	my( $expectedCount, $testName ) = @_;
	TestSlotAndResourceCount( $expectedCount, $testName, "SQUID" );
}

sub TestSlotAndResourceCount {
	my( $expectedCount, $testName, $resourceName ) = @_;

	my $ads = parseMachineAds( "Name", "Assigned${resourceName}s" );

	my %RESOURCEs;
	my $totalCount = 0;
	foreach my $ad (@{$ads}) {
		++$totalCount;
		my $assignedRESOURCEs = $ad->{ "Assigned${resourceName}s" };
		foreach my $RESOURCE (split( /[, ]+/, $assignedRESOURCEs )) {
			if( $RESOURCE =~ /^(${resourceName}\d)$/ ) {
				$RESOURCEs{ $RESOURCE } = 1;
			}
		}
	}

	if( $totalCount != $expectedCount ) {
		die( "Failure: Found ${totalCount} slots, was expecting $expectedCount.\n" );
	}

	if( scalar(keys %RESOURCEs) != 4 ) {
		die( "Failure: Found " . scalar(keys %RESOURCEs) . " ${resourceName}s, was expecting 4.\n" );
	}

	RegisterResult( 1, check_name => "slot-and-${resourceName}s-counts",
					   test_name => $testName );
}


#
# Verify that the individual UptimeSQUIDsSeconds are being reported properly.
#
sub TestUptimeSQUIDsSeconds {
	my( $testName ) = @_;
	TestUptimeResourceSeconds( $testName, "SQUID" );
}

sub TestUptimeResourceSeconds {
	my( $testName, $resourceName ) = @_;

	my $directAds = parseDirectMachineAds( "Assigned${resourceName}s", "Uptime${resourceName}sSeconds" );

	my $multiplier = undef;
	foreach my $ad (@{$directAds}) {
		my $value = $ad->{ "Uptime${resourceName}sSeconds" };
		if( $value eq "undefined" ) { next; }

		my $total = 0;
		my $assignedRESOURCEs = $ad->{ "Assigned${resourceName}s" };
		foreach my $RESOURCE (split( /[, ]+/, $assignedRESOURCEs )) {
			# my $increment = $RESOURCEIncrements{ $RESOURCE };
			my $increment = $increments{ $resourceName }->{ $RESOURCE };

			if(! defined( $increment )) {
				die( "Failure: Assigned ${resourceName} '" . $ad->{ "Assigned${resourceName}s" } . "' invalid.\n" );
			}

			$total += $increment;
		}

		# print( "UptimeSQUIDsSeconds ${value} % totalIncrement ${total}\n" );
		if( $value % $total != 0 ) {
			die( "Failure: '${assignedRESOURCEs}' has bad uptime '${value}'.\n" );
		}

		if(! defined( $multiplier )) {
			$multiplier = $value / $total;
			# print( "Inferring sample count = ${multiplier}.\n" );
		} else {
			if( $value / $total != $multiplier ) {
				die( "Failure: '${assignedRESOURCEs}' has a different sample count (" . $value / $total . " != " . $multiplier . ").\n" );
			}
		}
	}

	RegisterResult( 1, check_name => "Uptime${resourceName}sSeconds",
					   test_name => $testName );
}


#
# Callbacks for testing.
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

my %jobIDToRESOURCEsMap;
my $callbackResourceName;
my $executed = sub
{
	my %info = @_;
	my $ID = $info{ 'cluster' } . "." . $info{ 'job' };

	my $directAds = parseDirectMachineAds( "JobID", "Assigned${callbackResourceName}s" );
	foreach my $ad (@{$directAds}) {
		my $jobID = $ad->{ "JobID" };
		if(! defined($jobID)) { next; }
		if( $jobID ne $info{ 'cluster' } . "." . $info{ 'job' } ) { next; }

		$jobIDToRESOURCEsMap{ $jobID } = $ad->{ "Assigned${callbackResourceName}s" };
		TLOG( "Job $ID started on " . $jobIDToRESOURCEsMap{ $jobID } . "...\n" );
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

			if( $resource =~ m/${callbackResourceName}s/i ) {
				TLOG( "Job ${ID} used $usage $resource...\n" );
				$eventLogUsage{ $ID } = $usage;
			}
		}
	}

	if(! defined( $eventLogUsage{ $ID } )) {
		die( "Unable to find ${callbackResourceName}s usage in event log, aborting.\n" );
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

	my $STARTD_CRON_MONITOR_PERIOD = 10;
	my $expectedValue = $increment / $STARTD_CRON_MONITOR_PERIOD;
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
	return TestResourceUsage( $testName, "SQUID" );
}

# Instead of implementing $qf (for Queueing Factor) here and in
# TestResourceMemoryUsage(), we could also replace $resourceName and
# $submitFileFragment by instead accepting a hash of resource names
# mapped to request counts and computing $qf and $submitFileFragment
# to match; we could save even more time when running tests by
# arranging for the job to report on multiple resource usages
# (thus only having to call this function and TestResourceMemoryUsage()
# once each).
sub TestResourceUsage {
	my( $testName, $resourceName, $submitFileFragment, $qf ) = @_;

	$callbackResourceName = $resourceName;
	Condor::RegisterUsage( $usage );
	CondorTest::RegisterExitedAbnormal( $testName, $abnormal );
	CondorTest::RegisterAbort( $testName, $aborted );
	CondorTest::RegisterHold( $testName, $held );
	CondorTest::RegisterExecute( $testName, $executed );
	CondorTest::RegisterSubmit( $testName, $submitted );
	CondorTest::RegisterExitedSuccess( $testName, $success );
	CondorTest::RegisterEvictedWithoutCheckpoint( $testName, $on_evictedwithoutcheckpoint );
	CondorTest::RegisterExitedFailure( $testName, $failure );

	%jobIDToRESOURCEsMap = ();
	my %submitFileHash = (
		executable		=> 'x_sleep.pl',
		arguments		=> '53',
		output			=> 'cmr-monitor-basic.$(Cluster).$(Process).out',
		error			=> 'cmr-monitor-basic.$(Cluster).$(Process).err',
		log				=> 'cmr-monitor-basic.log',

		"request_${resourceName}s"		=> '1',
		LeaveJobInQueue					=> 'true',
		_queue							=> '4'
	);

	if( defined( $submitFileFragment ) ) {
		foreach my $key (keys %${submitFileFragment}) {
			$submitFileHash{ $key } = $submitFileFragment->{ $key };
		}
	}

	if(! defined( $qf )) { $qf = 1; }
	$submitFileHash{ _queue } = $submitFileHash{ _queue  } / $qf;

	if( CondorTest::RunTest2( name => $testName, submit_hash => \%submitFileHash, want_checkpoint => 0 ) ) {

		# For each job, check its history file for <Resource>Usage and see if
		# that value is sane, given what we know that the monitor is reporting
		# for each device.
		foreach my $jobID (keys %jobIDToRESOURCEsMap) {
			my $totalIncrement = 0;
			my $assignedResources = $jobIDToRESOURCEsMap{ $jobID };
			foreach my $resource (split( /[, ]+/, $assignedResources )) {
				my $increment = $increments{ $resourceName }->{ $resource };

				if(! defined( $increment )) {
					die( "Failure: resource '${resource}' invalid.\n" );
				}

				$totalIncrement += $increment;
			}

			my $jobAds = parseHistoryFile( $jobID, "${resourceName}sUsage" );
			if( scalar( @{$jobAds} ) != 1 ) {
				die( "Did not get exactly one (" . scalar( @{$jobAds} ) . ") job ad for job ID '${jobID}', aborting.\n" );
			}

			foreach my $ad (@{$jobAds}) {
				my $value = $ad->{ "${resourceName}sUsage" };

				# The event log's report is rounded for readability.
				my $elValue = sprintf( "%.2f", $value );
				if( $elValue != $eventLogUsage{ $jobID } ) {
					die( "Value in event log (" . $eventLogUsage{ $jobID } . ") does not equal value in history file (${value}), aborting.\n" );
				}

				if( checkUsageValue( $totalIncrement, $value ) ) {
					RegisterResult( 1, check_name => $assignedResources . "-usage", test_name => $testName );
				} else {
					RegisterResult( 0, check_name => $assignedResources . "-usage", test_name => $testName );
				}
			}
		}
	} else {
		die( "Error: $testName: CondorTest::RunTest(cmr-monitor-basic) failed\n" );
	}

	my $clusterID;
	my $setClusterID = sub {
		my( $cID ) = @_;
		$clusterID = $cID;
	};

	# Poll the .update.ad file and then check its results.
	%jobIDToRESOURCEsMap = ();
	%submitFileHash = (
		executable	=> 'cmr-monitor-basic-ad.pl',
		arguments	=> "53 ${resourceName}",
		output		=> 'cmr-monitor-basic-ad.$(Cluster).$(Process).out',
		error		=> 'cmr-monitor-basic-ad.$(Cluster).$(Process).err',
		log			=> 'cmr-monitor-basic-ad.log',

		"request_${resourceName}s"	=> '1',
		LeaveJobInQueue				=> 'true',
		_queue						=> '8'
	);

	if( defined( $submitFileFragment ) ) {
		foreach my $key (keys %${submitFileFragment}) {
			$submitFileHash{ $key } = $submitFileFragment->{ $key };
		}
	}

	if(! defined( $qf )) { $qf = 1; }
	$submitFileHash{ _queue } = $submitFileHash{ _queue  } / $qf;

	if( CondorTest::RunTest2( name => $testName, submit_hash => \%submitFileHash, want_checkpoint => 0, callback => $setClusterID ) ) {
		my $lineCount = 0;
		my $outputFileBaseName = "cmr-monitor-basic-ad.${clusterID}.";
		for( my $i = 0; $i < (8 / $qf); ++$i ) {
			my $outputFileName = $outputFileBaseName . $i . ".out";

			open( my $fh, '<', $outputFileName ) or
				die( "Error: ${testName}: could not open '${outputFileName}'\n" );

			while( my $line = <$fh> ) {
				++$lineCount;
				my( $RESOURCE, $value ) = split( ' ', $line );

				my $totalIncrement = 0;
				foreach my $resource (split( /[, ]+/, $RESOURCE )) {
					my $increment = $increments{ $resourceName }->{ $resource };

					if(! defined( $increment )) {
						die( "Failure: resource '${resource}' invalid.\n" );
					}

					$totalIncrement += $increment;
				}

				if( checkUsageValue( $totalIncrement, $value ) ) {
					RegisterResult( 1, check_name => $RESOURCE . "-update.ad-${lineCount}", test_name => $testName );
				} else {
					RegisterResult( 0, check_name => $RESOURCE . "-update.ad-${lineCount}", test_name => $testName );
				}
			}

			close( $fh );
		}

		# Each test job appends four lines to the log.
		if( $lineCount != (32 / $qf) ) {
			die( "Error: $testName: 'cmr-monitor-basic-ad.out' had $lineCount lines, not 32.\n" );
		}
	} else {
		die( "Error: $testName: CondorTest::RunTest(cmr-monitor-basic-ad) failed\n" );
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
	# my $result = CondorTest::runCondorTool( "condor_history ${jobID} -af ${attributeList}", \@lines, 2, { emit_output => 0 } );
	# There's an arbitrarily-long delay between when the job-terminated event
	# show up in the event log (and we think the job finishes) and when it
	# shows up in the history file.  Instead of dealing with that race, submit
	# the job with LeaveJobInQueue = true and run condor_q instead.
	my $result = CondorTest::runCondorTool( "condor_q ${jobID} -af ${attributeList}", \@lines, 2, { emit_output => 0 } );
	return parseAutoFormatLines( \@lines, \@attributes );
}

# -----------------------------------------------------------------------------

#
# This hash is from cmr-squid-monitor-memory and has to be changed when it does.
#
my %squidSequences = (
	"SQUID0" => [ 51, 51, 91, 11, 41, 41 ],
	"SQUID1" => [ 42, 42, 92, 12, 52, 52 ],
	"SQUID2" => [ 53, 53, 13, 93, 43, 43 ],
	"SQUID3" => [ 44, 44, 14, 94, 54, 54 ]
);

#
# This hash is from cmr-tako-monitor and has to be changed when it does.
#
my %takoSequences = (
	"TAKO0" => [ 5100, 5100, 9100, 1100, 4100, 4100 ],
	"TAKO1" => [ 4200, 4200, 9200, 1200, 5200, 5200 ],
	"TAKO2" => [ 5300, 5300, 1300, 9300, 4300, 4300 ],
	"TAKO3" => [ 4400, 4400, 1400, 9400, 5400, 5400 ]
);

my %sequences = (
	"SQUID"	=> \%squidSequences,
	"TAKO" => \%takoSequences
);

sub peaksAreAsExpected {
	my( $peaks, $expected ) = @_;

	if( scalar( @{$peaks} ) < scalar( @{$expected} ) ) {
		print( "Too few peaks (" . scalar( @{$peaks} ) . ") for number of expected values (" . scalar( @{$expected} ) . ").\n" );
		return 0;
	}

	for( my $i = 0; $i < scalar( @{$expected} ); ++$i ) {
		if( $peaks->[$i] != $expected->[$i] ) {
			return 0;
		}
	}

	return 1;
}

sub min {
	my( $a, $b ) = @_;
	if( $a < $b ) { return $a; } else { return $b; }
}

sub max {
	my( $a, $b ) = @_;
	if( $a > $b ) { return $a; } else { return $b; }
}

sub peaksMatchValues {
	my( $peaks, $offset, $values ) = @_;

	if( scalar( @{$peaks} ) < scalar( @{$values} ) ) {
		print( "Too few peaks (" . scalar( @{$peaks} ) . ") for number of given values (" . scalar( @{$values} ) . ").\n" );
		return 0;
	}

	my $size = scalar( @{$values} );
	my $expectedPeaks->[ 0 ] = $values->[$offset];
	for( my $i = 1; $i < $size; ++$i ) {
		$expectedPeaks->[ $i ] = max(
			$expectedPeaks->[ $i - 1 ],
			$values->[ ($offset + $i) % $size ]
		);
	}

	print( "Found peaks: " . join( " ", @{$peaks} ) . "\n" );
	print( "Found values (with offset $offset): " . join( " ", @{$values} ) . "\n" );
	print( "Computed expected peaks: " . join( " ", @{$expectedPeaks} ) . "\n" );

	return peaksAreAsExpected( $peaks, $expectedPeaks );
}

sub TestSQUIDsMemoryUsage {
	my( $testName ) = @_;
	return TestResourceMemoryUsage( $testName, "SQUID" );
}

sub TestResourceMemoryUsage {
	my( $testName, $resourceName, $submitFileFragment, $qf ) = @_;

	$callbackResourceName = $resourceName;
	CondorTest::RegisterExitedAbnormal( $testName, $abnormal );
	CondorTest::RegisterAbort( $testName, $aborted );
	CondorTest::RegisterHold( $testName, $held );
	CondorTest::RegisterExecute( $testName, $executed );
	CondorTest::RegisterSubmit( $testName, $submitted );
	CondorTest::RegisterExitedSuccess( $testName, $success );
	CondorTest::RegisterEvictedWithoutCheckpoint( $testName, $on_evictedwithoutcheckpoint );
	CondorTest::RegisterExitedFailure( $testName, $failure );

	my $clusterID;
	my $setClusterID = sub {
		my( $cID ) = @_;
		$clusterID = $cID;
	};

	%jobIDToRESOURCEsMap = ();
	my %submitFileHash = (
		executable	=> 'cmr-monitor-memory-ad.pl',
		arguments	=> "71 ${resourceName}",
		output		=> 'cmr-monitor-memory-ad.$(Cluster).$(Process).out',
		error		=> 'cmr-monitor-memory-ad.$(Cluster).$(Process).err',
		log			=> 'cmr-monitor-memory-ad.log',

		"request_${resourceName}s"	=> '1',
		LeaveJobInQueue				=> 'true',
		_queue						=> '8'
	);

	if( defined( $submitFileFragment ) ) {
		foreach my $key (keys %${submitFileFragment}) {
			$submitFileHash{ $key } = $submitFileFragment->{ $key };
		}
	}

	if(! defined( $qf )) { $qf = 1; }
	$submitFileHash{ _queue } = $submitFileHash{ _queue  } / $qf;

	if( CondorTest::RunTest2( name => $testName, submit_hash => \%submitFileHash, want_checkpoint => 0, callback => $setClusterID ) ) {
		my $lineCount = 0;
		my $outputFileBaseName = "cmr-monitor-memory-ad.${clusterID}.";
		for( my $i = 0; $i < (8 / $qf); ++$i ) {
			my $outputFileName = $outputFileBaseName . $i . ".out";

			open( my $fh, '<', $outputFileName ) or
				die( "Error: ${testName}: could not open '${outputFileName}'\n" );

			my $sequence = [];
			my $firstRESOURCE = undef;
			while( my $line = <$fh> ) {
				++$lineCount;
				my( $RESOURCE, $value ) = split( ' ', $line );
				if( $value eq "undefined" ) { next; }

				if(! defined( $firstRESOURCE )) {
					$firstRESOURCE = $RESOURCE;
				} else {
					if( $RESOURCE ne $firstRESOURCE ) {
						die( "Resources switched mid-job, aborting.\n" );
					}
				}

				push( @{$sequence}, $value );
			}

			my $properSequence = $sequences{ $resourceName }->{ $firstRESOURCE };
			if(! defined( $properSequence )) {
				print( "Did not find sequence for '$firstRESOURCE', constructing...\n" );
				my @ps;
				foreach my $resource (split( /[, ]+/, $firstRESOURCE )) {
					my $partialSequence = $sequences{ $resourceName }->{ $resource };
					print( "Found sequence '" . join( " ", @{$partialSequence} ) . "'\n" );
					if(! defined( $properSequence )) {
						@ps = @{$partialSequence};
					} else {
						for( my $i = 0; $i < scalar(@{$partialSequence}); ++$i ) {
							$ps[$i] = max( $properSequence->[$i], $partialSequence->[$i] );
						}
					}
				}
				$properSequence = \@ps;
				print( "Constructed sequence '" . join( " ", @{$properSequence} ) . "' for '$firstRESOURCE'.\n" );
			}

			my $offset = 0;
			my $length = min( scalar( @{$properSequence} ), scalar( @{$sequence} ) );
			for( ; $offset < $length; ++$offset ) {
				if( $properSequence->[$offset] == $sequence->[0] ) { last; }
			}
			if( $offset == $length ) {
				die( "Failed to find offset of '" . join( " ", @{$sequence} ) . "' in '" . join( " ", @{$properSequence} ) . "', aborting.\n" );
			}

			if( peaksMatchValues( $sequence, $offset, $properSequence ) ) {
				RegisterResult( 1, check_name => $firstRESOURCE . "-sequence-${i}", test_name => $testName );
			} elsif( $offset == 0 && peaksMatchValues( $sequence, $offset + 1, $properSequence ) ) {
				RegisterResult( 1, check_name => $firstRESOURCE . "-sequence-${i}", test_name => $testName );
			} elsif( $offset == 4 && peaksMatchValues( $sequence, $offset + 1, $properSequence ) ) {
				RegisterResult( 1, check_name => $firstRESOURCE . "-sequence-${i}", test_name => $testName );
			} else {
				RegisterResult( 0, check_name => $firstRESOURCE . "-sequence-${i}", test_name => $testName );
			}

			close( $fh );
		}
	} else {
		die( "Error: $testName: CondorTest::RunTest(cmr-monitor-memory) failed\n" );
	}
}

1;
