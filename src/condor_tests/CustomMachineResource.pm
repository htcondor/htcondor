package CustomMachineResource;

# use Exporter;
# our @ISA = qw( Exporter );
# our @EXPORT = qw( ... );

use strict;
use warnings;

use CondorTest;

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

1;
