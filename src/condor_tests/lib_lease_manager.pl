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
sub DaemonWait( $ );
sub RunTests( $ );
sub RunTestOp( $$ );
sub CountLiveLeases( $ );

my $testname = 'lib_lease_manager - runs lease manager tests';
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
		 config =>
		 {
			 GETADS_INTERVAL			=> 10,
			 UPDATE_INTERVAL			=> 10,
			 PRUNE_INTERVAL				=> 60,
			 DEBUG_ADS					=> "TRUE",
			 MAX_LEASE_DURATION			=> 60,
			 MAX_TOTAL_LEASE_DURATION	=> 120,
			 DEFAULT_MAX_LEASE_DURATION	=> 60,

			 QUERY_ADTYPE				=> "Any",
			 CLASSAD_LOG				=> "\$(SPOOL)/LeaseAdLog",

			 ADVERTISER					=> $programs{advertise},
			 ADVERTISER_LOG				=> "\$(LOG)/AdvertiserLog",

			 LEASEMANAGER				=> "\$(SBIN)/condor_lease_manager",
			 LEASEMANAGER_LOG			=> "\$(LOG)/LeaseManagerLog",
			 #LeaseManager_ARGS			=> "-local-name any",
			 #LeaseManager_ARGS			=> "-f",
			 LEASEMANAGER_DEBUG			=> "D_FULLDEBUG D_CONFIG",
			 DAEMON_LIST	=> "MASTER, COLLECTOR, LEASEMANAGER, ADVERTISER",
			 DC_DAEMON_LIST				=> "+ LEASEMANAGER",

		 },
		 advertise =>
		 {
			 command					=> "UPDATE_AD_GENERIC",
			 "--period"					=> 60,
		 },
		 resource =>
		 {
			 MyType						=> "Generic",
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
	 defaults =>
	 {
		 config => { },
		 advertise => { },
		 resource => { },
		 advertise => { },
		 run =>
		 {
			 loops =>
			 [
			  {
				  op			=> [ "GET", 60, 1 ],
				  expect		=> 1,
				  sleep			=> 10,
			  },
			  {
				  op			=> [ "RELEASE", "'*'" ],
				  expect		=> 0,
			  }
			  ],
		 },
     },

     # Advertise try to grab 2
	 simple1 =>
     {
		 config => { },
		 advertise => {
			 MaxLeases			=> 1,
		 },
		 resource => { },
		 advertise => { },
		 run =>
		 {
			 loops =>
			 [
			  {
				  op			=> [ "GET", 60, 2 ],
				  expect		=> 1,
				  sleep			=> 10,
			  },
			  {
				  op			=> [ "RELEASE", "'*'" ],
				  expect		=> 0,
			  }
			  ],
		 },
     },

     # Advertise try to grab 2
	 simple2 =>
     {
		 config => { },
		 advertise => {
			 MaxLeases			=> 1,
		 },
		 resource => { },
		 advertise => { },
		 run =>
		 {
			 loops =>
			 [
			  {
				  op			=> [ "GET", 60, 1 ],
				  expect		=> 1,
				  sleep			=> 10,
			  },
			  {
				  op			=> [ "GET", 60, 1 ],
				  expect		=> 1,
				  sleep			=> 10,
			  },
			  {
				  op			=> [ "RELEASE", "'*'" ],
				  expect		=> 0,
			  }
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
	"  -d|--debug    enable D_FULLDEBUG debugging\n" .
	"  -v|--verbose  increase verbose level\n" .
	"  -s|--stop     stop after errors\n" .
	"  --valgrind    run test under valgrind\n" .
	"  -q|--quiet    cancel debug & verbose\n" .
	"  -h|--help     this help\n";
}


my %settings =
(
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
    elsif ( $arg eq "-s"  or  $arg eq "--stop" ) {
		$settings{stop} = 1;
    }
    elsif ( $arg eq "--valgrind" ) {
		$settings{valgrind} = 1;
    }
    elsif ( $arg eq "-v"  or  $arg eq "--verbose" ) {
		$settings{verbose}++;
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
			usage( );
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


my %test;
FillTest( \%test, $tests{common} );
FillTest( \%test, $settings{test} );
$test{name} = $settings{name};

my $dir = "test-lease_manager-" . $settings{name};
my $fulldir = getcwd() . "/$dir";
if ( -d $dir  and  $settings{force} ) {
    system( "/bin/rm", "-fr", $dir );
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

my $config = "$dir/condor_config.local";
open( CONFIG, ">$config" ) or die "Can't write to config file $config";
foreach my $param ( sort keys(%{$test{config}}) ) {
    my $value = $test{config}{$param};
    if ( $value eq "TRUE" ) {
		print CONFIG "$param = TRUE\n";
    }
    elsif ( $value eq "FALSE" ) {
		print CONFIG "$param = FALSE\n";
    }
    else {
		print CONFIG "$param = $value\n";
    }
}
print CONFIG "ADVERTISER_ARGS = " . join( " ", @advertiser_args ) . "\n";
close( CONFIG );

if ( not $settings{execute} ) {
	exit( 0 );
}

$test{client}->{lease_file} = "$fulldir/leases.state";

# This is all throw-away
my $main_config = "$fulldir/condor_config";
system( "cp condor_config.nrl $main_config" );

$ENV{CONDOR_CONFIG} = "$main_config";

my $log = `condor_config_val log`; chomp $log;
mkdir( $log ) or die "Can't create $log";
my $spool = `condor_config_val spool`; chomp $spool;
mkdir( $spool ) or die "Can't create $spool";

system( "condor_master" );

if (! DaemonWait( $test{resource}{MaxLeases} ) ) {
}

print $test{client}->{lease_file} . "\n";;
RunTests( \%test );

system( "condor_off -master" );
# end throw-away code

#CondorTest::debug("About to set up Condor Personal : <<<", 1);
#system("date");
#CondorTest::debug(">>>\n", 1);

# get a remote scheduler running (side b)
#my $configrem = CondorPersonal::StartCondor("x_param.wantcore" ,"wantcore");


my @valgrind = ( "valgrind", "--tool=memcheck", "--num-callers=24",
				 "-v", "-v", "--leak-check=full" );

system("date");
if ( $settings{verbose} ) {
    print "Using directory $dir\n";
}
my $errors = 0;

# Wait for everything to come to life...
sub DaemonWait( $ )
{
	my $MinResources = shift;

	print "Waiting for all daemons and $MinResources resource to show up\n";
	sleep( 2 );

	my $NumResources = 0;
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
						$ads{$MyType}{$1} = $2;
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
			$NumResources = $ads{LeaseManager}{NumberResources};
			if ( $NumResources >= $MinResources ) {
				return 1;
			}
			print "waiting for resources ($NumResources)\n";
			sleep( 5 );
		}
	}
	if ( scalar(@missing) ) {
		print STDERR "missing ads: " . join( " ", @missing ) . "\n";
		return 0;
	}
	else {
		print STDERR
			"Too few resources found: $NumResources < $MinResources\n";
		return 0;
	}
}

sub RunTests( $ )
{
	my $test = shift;

	my $run = $test->{run};
	for my $loop ( @{$run->{loops}} ) {
		my $leases;
		if ( exists $loop->{op} ) {
			$leases = RunTestOp( $test, $loop->{op} );
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
		if ( exists $loop->{sleep} ) {
			if ( $settings{verbose} ) {
				printf "Sleeping for %d seconds..\n", $loop->{sleep};
			}
			sleep( $loop->{sleep} );
		}
	}
}

sub RunTestOp( $$ )
{
	my $test = shift;
	my $op = shift;

	my @cmd;
	push( @cmd, $programs{tester} );
	push( @cmd, $test->{client}{lease_file} );
	foreach my $n ( 0 .. $settings{verbose} ) {
		push( @cmd, "-v" );
	}
	foreach my $t ( @{$op} ) {
		push( @cmd, $t );
	}
	my $cmdstr = join( " ", @cmd );
	if ( $settings{verbose} ) {
		print "Runing: $cmdstr\n";
	}

	if ( !open( TESTER, "$cmdstr|" ) ) {
		print STDERR "Can't run $cmdstr\n";
		return undef;
	}

	my @leases;
	while( <TESTER> ) {
		chomp;
		if ( /LEASE (\d+) \{/ ) {
			my %lease;
			while( <TESTER> ) {
				chomp;
				if (/^\s*\}/ ) {
					push( @leases, \%lease );
				}
				elsif ( /^\s+(\w+)=(.+)/ ) {
					$lease{$1} = $2;
				}
				else {
					# skip
				}
			}
		}
	}
	my $status = close( TESTER );
	print "tester status: $status\n";
	if ( ($status>>8) ) {
		printf STDERR "tester failed: %d\n", ($status >> 8);
		return undef;
	}
	print "found ". scalar(@leases) . " leases\n";

	return \@leases;
}

sub CountLiveLeases( $ )
{
	my $leases = shift;

	my $count = 0;
	foreach my $lease ( @{$leases} ) {
		if ( ( $lease->{dead}    =~ /true/i ) ||
			 ( $lease->{expired} =~ /true/i ) ) {
			next;
		}
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
