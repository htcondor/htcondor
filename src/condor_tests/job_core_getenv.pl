#! /usr/bin/env perl


my $arg = $ARGV[0];

my $cfig = $ENV{CONDOR_CONFIG};
my $pfig = $ENV{PATH};
my $ufig = $ENV{UNIVERSE};

if ( $cfig eq undef )
{
	print "{$arg}CONDOR_CONFIG is undefined\n";
	if( $arg ne "failok" ) { exit(1); }
}
else
{
	print "{$arg}CONDOR_CONFIG is $cfig\n";
	if( $arg ne "failnotok" ) { exit(1); }
}

if ( $pfig eq undef )
{
	print "{$arg}PATH is undefined\n";
	if( $arg ne "failok" ) { exit(1); }
}
else
{
	print "{$arg}PATH is $pfig\n";
	if( $arg ne "failnotok" ) { exit(1); }
}

if ( $ufig eq undef )
{
	print "{$arg}UNIVERSE is undefined\n";
	if( $arg ne "failok" ) { exit(1); }
}
else
{
	print "{$arg}UNIVERSE is $ufig\n";
	if( $arg ne "failnotok" ) { exit(1); }
}

