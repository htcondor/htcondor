#! /usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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

use strict;
use warnings;
use Cwd;
use CondorPersonal;
use CondorTest;

# 'Prototypes'
sub FillTest($$);
sub WriteConfig( $ );
sub WriteConfigSection( $$$ );
sub DaemonWait( $ );
sub RunTests( $ );
sub RunTestOp( $$$ );
sub CountLiveLeases( $ );

my $version = "1.1.0";
my $testdesc =  'lib_lease_manager - runs lease manager tests';
my $testname = "lib_lease_manager";
my $testbin = "../testbin_dir";
my %programs = ( advertise	=> getcwd() . "/x_advertise.pl",
				 tester => "$testbin/condor_lease_manager_tester",
				 );

my $now = time( );
my %tests =
    (

     # Common to all tests
     common =>
     {
		 config_lm =>
		 {
			 GETADS_INTERVAL			=> 10,
			 UPDATE_INTERVAL			=> 10,
			 PRUNE_INTERVAL				=> 60,
			 DEBUG_ADS					=> "TRUE",
			 MAX_LEASE_DURATION			=> 60,
			 MAX_TOTAL_LEASE_DURATION	=> 120,
			 DEFAULT_MAX_LEASE_DURATION	=> 60,

			 QUERY_ADTYPE				=> "Generic",
			 QUERY_CONSTRAINTS	=> "target.MyType == \"LeaseResource\"",
			 #QUERY_CONSTRAINTS	=> target.TargetType == "SomeType",

			 CLASSAD_LOG				=> "\$(SPOOL)/LeaseAdLog",
		 },
		 config =>
		 {
			 KEEP_POOL_HISTORY			=> "False",

			 ADVERTISER					=> $programs{advertise},
			 ADVERTISER_LOG				=> "\$(LOG)/AdvertiserLog",

			 LEASEMANAGER				=> "\$(SBIN)/condor_lease_manager",
			 LEASEMANAGER_LOG			=> "\$(LOG)/LeaseManagerLog",
			 #LeaseManager_ARGS			=> "-local-name any",
			 #LeaseManager_ARGS			=> "-f",
			 LEASEMANAGER_DEBUG			=> "D_FULLDEBUG D_CONFIG",
			 DC_DAEMON_LIST				=> "+ LEASEMANAGER",

			 TOOL_DEBUG					=> "D_FULLDEBUG",
			 ALL_DEBUG					=> "",
		 },
		 advertise =>
		 {
			 command					=> "UPDATE_AD_GENERIC",
			 "--period"					=> 60,
		 },
		 resource =>
		 {
			 MyType						=> "LeaseResource",
			 Name						=> "resource-1",
			 MaxLeases					=> 1,
			 DaemonStartTime			=> $now,
			 LeaseDuration				=> 60,
			 MaxLeaseDuration			=> 120,
			 UpdateSequenceNumber		=> 1,
		 },
		 client => { },
     },

	 # Simple default test --
	 # idential configuration, just grab one lease & free it
	 simple =>
	 {
		 config => { },
		 config_lm => { },
		 advertise => { },
		 resource => {
			 MaxLeases => 1,
		 },
		 advertise => { },
		 run =>
		 {
			 loops =>
			 [
			  { op => [ "GET", 60, 1 ],   expect => 1, },
			  { op => [ "RELEASE", "*" ], expect => 0, }
			  ],
		 },
     },

     # Advertise 1, try to grab 2
	 get_rel_1 =>
     {
		 config => { },
		 config_lm => { },
		 advertise => { },
		 resource => { 
			 MaxLeases => 1,
		 },
		 advertise => { },
		 run =>
		 {
			 loops =>
			 [
			  { op => [ "GET", 60, 2 ],   expect => 1, },
			  { op => [ "RELEASE", "*" ], expect => 0, }
			  ],
		 },
     },

     # Advertise 1, try to grab 2
	 get_rel_2 =>
     {
		 config => { },
		 config_lm => { },
		 advertise => { },
		 resource => {
			 MaxLeases => 1,
		 },
		 advertise => { },
		 run =>
		 {
			 loops =>
			 [
			  { op => [ "GET", 60, 1 ],   expect => 1, },
			  { op => [ "GET", 60, 1 ],   expect => 1, },
			  { op => [ "RELEASE", "*" ], expect => 0, }
			  ],
		 },
     },

     # Advertise 1, try to grab 2
	 get_rel_3 =>
     {
		 config => { },
		 config_lm => { },
		 advertise => { },
		 resource => {
			 MaxLeases => 2,
		 },
		 advertise => { },
		 run =>
		 {
			 loops =>
			 [
			  { op => [ "GET", 60, 1 ],   expect => 1, },
			  { op => [ "GET", 60, 1 ],   expect => 2, },
			  { op => [ "RELEASE", "*" ], expect => 0, }
			  ],
		 },
     },

     # Advertise 5, try to grab 5, release 2
	 get_rel_4 =>
     {
		 config => { },
		 config_lm => { },
		 advertise => { },
		 resource => {
			 MaxLeases => 5,
		 },
		 advertise => { },
		 run =>
		 {
			 loops =>
			 [
			  { op => [ "GET", 60, 5 ], expect => 5, },
			  { op => [ "RELEASE", "-d", "[0]", "[1]" ], expect => 3, }
			  ],
		 },
     },

     # Advertise 5, a lot of gets & releases
	 get_rel_5 =>
     {
		 config => { },
		 config_lm => { },
		 advertise => { },
		 resource => {
			 MaxLeases => 5,
		 },
		 advertise => { },
		 run =>
		 {
			 loops =>
			 [
			  { op => [ "GET", 60, 2 ],					 expect => 2, },
			  { op => [ "RELEASE", "-d", "[0]" ],		 expect => 1, },
			  { op => [ "GET", 60, 2 ],					 expect => 3, },
			  { op => [ "RELEASE", "-d", "[0]", "[1]" ], expect => 1, },
			  { op => [ "GET", 60, 4 ],					 expect => 5, },
			  { op => [ "GET", 60, 5 ],					 expect => 5, },
			  { op => [ "RELEASE", "-d", "[1]" ],		 expect => 4, },
			  { op => [ "GET", 60, 5 ],					 expect => 5, },
			  { op => [ "RELEASE", "-d", "[0]", "[1]" ], expect => 3, },
			  { op => [ "GET", 60, 5 ],					 expect => 5, },
			  { op => [ "RELEASE", "-d", "*" ],			 expect => 0, },
			  ],
		 },
     },

     # Renew leases
	 renew_1 =>
     {
		 config => { },
		 config_lm => { },
		 advertise => { },
		 resource => {
			 LeaseDuration		=> 60,
			 MaxLeaseDuration	=> 240,
			 MaxLeases			 => 5,
		 },
		 advertise => { },
		 run =>
		 {
			 loops =>
			 [
			  { op => [ "GET", 60, 5 ],			expect => 5, sleep => 30 },
			  { op => [ "RENEW", 60, "*" ], 	expect => 5, sleep => 30 },
			  { op => [ "RENEW", 60, "*" ], 	expect => 5, sleep => 30 },
			  { op => [ "RENEW", 60, "*" ], 	expect => 5, sleep => 30 },
			  ],
		 },
     },
);

sub usage( )
{
    print
		"usage: $0 [options] test-name\n" .
		"  -l|--list     list names of tests\n" .
		"  -f|--force    force overwrite of test directory\n" .
		"  -n|--no-exec  no execute / test mode\n" .
		"  -p|--pid      append PID to test directory\n" .
		"  -N|--no-pid   do not append PID to test directory\n" .
		"  -d|--debug    enable D_FULLDEBUG debugging\n" .
		"  -v|--verbose  increase verbose level\n" .
		"  -s|--stop     stop after errors\n" .
		"  --valgrind    run test under valgrind\n" .
		"  --version     print version information and exit\n" .
		"  -q|--quiet    cancel debug & verbose\n" .
		"  -h|--help     this help\n";
}


my %settings =
(
 use_pid		=> 0,
 verbose		=> 0,
 debug			=> 0,
 execute		=> 1,
 stop			=> 0,
 force			=> 0,
 valgrind		=> 0,
 );
foreach my $arg ( @ARGV ) {
    if ( $arg eq "-f"  or  $arg eq "--force" ) {
		$settings{force} = 1;
    }
    elsif ( $arg eq "-l"  or  $arg eq "--list" ) {
		foreach my $test ( sort keys (%tests) ) {
			print "  " . $test . "\n" if ( $test ne "common" );
		}
		exit(0);
    }
    elsif ( $arg eq "-n"  or  $arg eq "--no-exec" ) {
		$settings{execute} = 0;
    }
    elsif ( $arg eq "-p"  or  $arg eq "--pid" ) {
		$settings{use_pid} = 1;
    }
    elsif ( $arg eq "-N"  or  $arg eq "--no-pid" ) {
		$settings{use_pid} = 0;
    }
    elsif ( $arg eq "-s"  or  $arg eq "--stop" ) {
		$settings{stop} = 1;
    }
    elsif ( $arg eq "--valgrind" ) {
		$settings{valgrind} = 1;
    }
    elsif ( $arg eq "-v"  or  $arg eq "--verbose" ) {
		$settings{verbose}++;
    }
    elsif ( $arg eq "--version" ) {
		printf "Lease Manager Test version %s\n", $version;
		exit( 0 );
    }
    elsif ( $arg eq "-d"  or  $arg eq "--debug" ) {
		$settings{debug} = 1;
    }
    elsif ( $arg eq "-q"  or  $arg eq "--quiet" ) {
		$settings{verbose} = 0;
		$settings{debug} = 0;
    }
    elsif ( $arg eq "-h"  or  $arg eq "--help" ) {
		usage( );
		exit(0);
    }
    elsif (  !($arg =~ /^-/)  and  !exists($settings{name})  ) {
		$settings{name} = $arg;
		if ( !exists $tests{$settings{name}} ) {
			print STDERR "Unknown test name: $settings{name}\n";
			print STDERR "  use --list to list tests\n";
			exit(1);
		}
		$settings{test} = $tests{$settings{name}};
    }
    else {
		print STDERR "Unknown argument $arg\n";
		usage( );
		exit(1);
    }
}
if ( !exists $settings{name} ) {
    usage( );
    exit(1);
}

#CondorPersonal::DebugLevel(3);

my %test;
FillTest( \%test, $tests{common} );
FillTest( \%test, $settings{test} );
$test{name} = $settings{name};
my $full_name = "lease_manager-".$settings{name};

my $dir = "test-lease_manager-" . $settings{name};
if ( $settings{use_pid} ) {
	$dir .= ".$$";
}

my $fulldir = getcwd() . "/$dir";
if ( -d $dir  and  $settings{force} ) {
    system( "rm", "-fr", $dir );
}
if ( -d $dir ) {
    die "Test directory $dir already exists! (try --force?)";
}
mkdir( $dir ) or die "Can't create directory $dir";
print "writing to $dir\n" if ( $settings{verbose} );

# Generate the ad to publish
my $resource = "$fulldir/resource.ad";
open( AD, ">$resource" ) or
	die "Can't write to resource ClassAd file $resource";
foreach my $attr ( keys(%{$test{resource}}) ) {
    my $value = $test{resource}{$attr};
    if ( $value eq "TRUE" ) {
		print AD "$attr = TRUE\n";
    }
    elsif ( $value eq "FALSE" ) {
		print AD "$attr = FALSE\n";
    }
	elsif ( $value =~ /^\d+$/ ) {
		print AD "$attr = $value\n";
	}
	elsif ( $value =~ /^\d+\.\d+$/ ) {
		print AD "$attr = $value\n";
	}
    else {
		print AD "$attr = \"$value\"\n";
    }
}
close( AD );

# Generate the advertiser command line
my @advertiser_args;
foreach my $key ( keys(%{$test{advertise}}) ) {
	my $value = $test{advertise}{$key};

	if ( $key =~ /^-/ ) {
		push( @advertiser_args, $key );
		if ( defined($value) ) {
			push( @advertiser_args, $value );
		}
		if ( $settings{valgrind} ) {
			push( @advertiser_args, "--valgrind" );
		}
    }
}
push( @advertiser_args, "--delay" );
push( @advertiser_args, 5 );
foreach my $n ( 0 .. $settings{verbose} ) {
	push( @advertiser_args, "-v" );
}
push( @advertiser_args, $test{advertise}{command} );
push( @advertiser_args, $resource );

my $config_tmp = "$dir/condor_config.tmp";
WriteConfig( $config_tmp );

if ( not $settings{execute} ) {
	exit( 0 );
}

$test{client}->{lease_file} = "$fulldir/leases.state";

my $param_file = "$fulldir/params";
open( PARAMS, ">$param_file" ) or die "can't write to $param_file";
print PARAMS <<ENDPARAMS;
ports = dynamic
condor = nightlies
localpostsrc = $config_tmp
daemon_list = MASTER,COLLECTOR,LEASEMANAGER,ADVERTISER
personaldir = $fulldir
ENDPARAMS
close(PARAMS);

my $time_start = time();
my $ltime = localtime( $time_start );
printf( "\n** Lease Manager test v%s starting test %s @ %s **\n\n",
		$version, $settings{name}, $ltime );

CondorTest::debug("About to set up Condor Personal : <<<$ltime>>>\n", 1);

# Fire up my condor
my $configrem =	CondorTest::StartPersonal($full_name, $param_file, $version);
my ($config, $port) = split( /\+/, $configrem );
$ENV{CONDOR_CONFIG} = $config;
print "Condor config: '" . $ENV{CONDOR_CONFIG} . "'\n";

if (! DaemonWait( $test{resource}{MaxLeases} ) ) {
	print STDERR "Daemons / leases didn't start properly\n";
	system( 'condor_status -any -l' );
}
else {
	print $test{client}->{lease_file} . "\n";
	printf "* Tests starting @ %s *\n", $ltime = localtime();
	RunTests( \%test );
	printf "* Tests complete @ %s *\n", $ltime = localtime();
}

CondorTest::KillPersonal( $config );

my $time_end = time();
$ltime = localtime( $time_end );
printf( "\n** Test %s @ finished @ %s (%ss) **\n",
		$settings{name}, $ltime, $time_end - $time_start );
if ( $settings{verbose} ) {
    print "Used directory $dir\n";
}
my $errors = 0;


sub FillTest($$)
{
	my $dest = shift;
	my $src  = shift;
	foreach my $section ( keys %{$src} )
	{
		if (! exists $dest->{$section} ) {
			$dest->{$section} = {};
		}
		foreach my $attr ( keys(%{$src->{$section}})  ) {
			$dest->{$section}{$attr} = $src->{$section}{$attr};
		}
    }
}

sub WriteConfig( $ )
{
	my $config = shift;

	my $fh;
	open( $fh, ">$config" ) or die "Can't write to config file $config";
	print $fh "#\n";
	print $fh "# auto-generated by lib_lease_manager.pl\n";
	print $fh "#\n";
	WriteConfigSection( $fh, "", $test{config} );
	WriteConfigSection( $fh, "LeaseManager.", $test{config_lm} );
	print $fh "ADVERTISER_ARGS = " . join( " ", @advertiser_args ) . "\n";
	print $fh "#\n";
	print $fh "# End of auto-generated by lib_lease_manager.pl\n";
	print $fh "#\n";
	close( $fh );
}

sub WriteConfigSection( $$$ )
{
	my $fh = shift;
	my $prefix = shift;
	my $hashref = shift;

	foreach my $param ( sort keys(%{$hashref}) ) {
		my $value = $hashref->{$param};
		if ( $value eq "TRUE" ) {
			print $fh "$prefix$param = TRUE\n";
		}
		elsif ( $value eq "FALSE" ) {
			print $fh "$prefix$param = FALSE\n";
		}
		else {
			print $fh "$prefix$param = $value\n";
		}
	}
}

# Wait for everything to come to life...
sub DaemonWait( $ )
{
	my $MinLeases = shift;

	print "Waiting for all daemons and $MinLeases leases to show up\n";
	sleep( 2 );

	my $NumLeases = 0;
	my @missing;
	my %ads;
	for my $tries ( 0 .. 10 ) {

		my %found = ( DaemonMaster=>0, LeaseManager=>0, Generic=>0 );
		open( STATUS, "condor_status -any -l|" )
			or die "Can't run condor_status";
		while( <STATUS> ) {
			chomp;
			if ( /^MyType = \"(\w+)\"/ ) {
				my $MyType = $1;
				$found{$MyType} = 1;
				$ads{$MyType} = { MyType => $MyType };
				while( <STATUS> ) {
					chomp;
					if ( /^(\w+) = \"(.*)\"/ ) {
						$ads{$MyType}{$1} = $2;
					}
					elsif ( /^(\w+) = (.*)/ ) {
						my $attr = $1;
						my $value = $2;
						$ads{$MyType}{$attr} = $value;
					}
					elsif ( $_ eq "" ) {
						last;
					}
				}
			}
		}
		close( STATUS );
		$#missing = -1;
		foreach my $d ( keys %found ) {
			if ( ! $found{$d} ) {
				push( @missing, $d );
			}
		}
		if ( scalar(@missing) ) {
			if ( $settings{verbose} > 1 ) {
				print "missing ads: " . join( " ", @missing ) . "\n";
			}
			sleep( 5 );
		}
		if ( exists $ads{LeaseManager} ) {
			$NumLeases = $ads{LeaseManager}{NumberValidLeases};
			if ( $NumLeases >= $MinLeases ) {
				return 1;
			}
			sleep( 5 );
		}
	}
	if ( scalar(@missing) ) {
		print STDERR "missing ads: " . join( " ", @missing ) . "\n";
		return 0;
	}
	else {
		print STDERR
			"Too few leases found: $NumLeases < $MinLeases\n";
		return 0;
	}
}

sub RunTests( $ )
{
	my $test = shift;

	my $leases;
	my $run = $test->{run};
	for my $loop ( @{$run->{loops}} ) {
		print "\n";
		if ( exists $loop->{op} ) {
			$leases = RunTestOp( $test, $loop->{op}, $leases );
			if ( ! defined($leases) ) {
				$errors++;
				$leases = [];
			}
		}
		if ( exists $loop->{expect} ) {
			my $count = CountLiveLeases( $leases );
			if ( $count != $loop->{expect} ) {
				printf( "ERROR: lease count mis-match: expect %d, found %d\n",
						$loop->{expect}, $count );
				$errors++;
			}
			else {
				printf( "Found correct # of leases\n" );
			}
		}

		printf "%d leases:\n", scalar(@{$leases});
		foreach my $n ( 0 .. $#{$leases} ) {
			my $lease = @{$leases}[$n];
			if ( $settings{verbose} ) {
				for my $k ( keys %{$lease} ) {
					print "$k=".$lease->{$k}." ";
				}
				print "\n";
			}
			elsif (  (not $lease->{Dead})  and  (not $lease->{Expired}) ) {
				printf( "Lease %02d: %s %s %ds %ds\n",
						$n,
						$lease->{Resource},
						$lease->{LeaseID},
						$lease->{Duration},
						$lease->{Remaining} );
			}
		}
		if ( exists $loop->{sleep} ) {
			if ( $settings{verbose} ) {
				printf "Sleeping for %d seconds..\n", $loop->{sleep};
			}
			sleep( $loop->{sleep} );
		}
		else {
			#sleep( 1 );
		}
	}
}

sub RunTestOp( $$$ )
{
	my $test = shift;
	my $op = shift;
	my $leases = shift;

	my @cmd;

	my @valgrind = ( "valgrind", "--tool=memcheck", "--num-callers=24",
					 "-v", "-v", "--leak-check=full" );
	if ( $settings{valgrind} ) {
		for my $t ( @valgrind ) {
			push( @cmd, $t );
		}
		push( @cmd, "--log-file=$fulldir/tester.valgrind" );
	}

	push( @cmd, $programs{tester} );
	push( @cmd, $test->{client}{lease_file} );
	foreach my $n ( 0 .. $settings{verbose} ) {
		push( @cmd, "-v" );
	}
	my @show;
	foreach my $t ( @{$op} ) {
		if ( $t =~ /\[(\d+)\]/ ) {
			my $n = $1;
			if ( $n > $#{$leases} ) {
				print STDERR "ERROR: Lease $n doesn't exist!!!\n";
				push( @show, "$t=>'NONE'" );
				$errors++;
			}
			else {
				my $id = @{$leases}[$n]->{LeaseID};
				push( @cmd, $id );
				push( @show, "$t=>'$id'" );
			}
		}
		elsif ( $t eq "*" ) {
			push( @cmd, "'*'" );
			push( @show, $t );
		}
		else {
			push( @cmd, $t );
			push( @show, $t );
		}
	}
	print "Command: " . join(" ", @show) . "\n";

	my $cmdstr = join( " ", @cmd );
	if ( $settings{verbose} ) {
		print "Runing: $cmdstr\n";
	}

	if ( !open( TESTER, "$cmdstr|" ) ) {
		print STDERR "Can't run $cmdstr\n";
		return undef;
	}

	my @new_leases;
	while( <TESTER> ) {
		chomp;
		if ( /LEASE (\d+) \{/ ) {
			my $lease = ();
			while( <TESTER> ) {
				chomp;
				if (/^\s*\}/ ) {
					push( @new_leases, $lease );
					$lease = ();
				}
				elsif ( /^\s+(\w+)=(.+)/ ) {
					my $attr = $1;
					my $value = $2;
					$value = 1 if ( $value =~ /^TRUE$/i );
					$value = 0 if ( $value =~ /^FALSE$/i );
					$lease->{$attr} = $value;
				}
				else {
					# skip
				}
			}
		}
	}
	my $status = close( TESTER );
	if ( ($status>>8) ) {
		printf STDERR "tester failed: %d\n", ($status >> 8);
		return undef;
	}
	print "found ". scalar(@new_leases) . " leases\n";

	return \@new_leases;
}

sub CountLiveLeases( $ )
{
	my $leases = shift;

	my $count = 0;
	foreach my $lease ( @{$leases} ) {
		next if ( $lease->{Dead} or $lease->{Expired} );
		$count++;
	}
	return $count;
}

sub CheckValgrind( $$ )
{
    my $errors = 0;
    my $file = shift;

    my $full;
    opendir( DIR, $dir ) or die "Can't opendir $dir";
    while( my $f = readdir(DIR) ) {
		if ( index( $f, $file ) >= 0 ) {
			$full = "$dir/$f";
			last;
		}
    }
    closedir( DIR );
    if ( !defined $full ) {
		print STDERR "Can't find valgrind log for $file";
		return 1;
    }

    if ( ! open( VG, $full ) ) {
		print STDERR "Can't read valgrind output $full\n";
		return 1;
    }
    if ( $settings{verbose} ) {
		print "Reading valgrind output $full\n";
    }
    my $dumped = 0;
    while( <VG> ) {
		if ( /ERROR SUMMARY: (\d+)/ ) {
			my $e = int($1);
			if ( $e ) {
				$errors++;
			}
			if ( $settings{verbose}  or  $e ) {
				if ( !$dumped ) {
					print "$full:\n";
					$dumped++;
				}
				print "  $_";
			}
		}
		elsif ( /LEAK SUMMARY:/ ) {
			my @lines = ( $_ );
			my $leaks = 0;
			while( <VG> ) {
				if ( /blocks\.$/ ) {
					push( @lines, $_ );
				}
				if ( /lost: (\d+)/ ) {
					if ( int($1) > 0 ) {
						$leaks++;
						$errors++;
					}
				}
			}
			if ( $settings{verbose}  or  $leaks ) {
				if ( !$dumped ) {
					print "$full:\n";
					$dumped++;
				}
				foreach $_ ( @lines ) {
					print "  $_";
				}
			}
		}

    }
    close( VG );
    return $errors;
}


### Local Variables: ***
### mode:perl ***
### tab-width: 4  ***
### End: ***
