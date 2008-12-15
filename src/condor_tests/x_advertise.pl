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
use File::Temp qw/ tempfile tempdir /;

# open( STDOUT, "> /tmp/x_advertise.log" ) or die "Can't open stdout";
# open( STDERR, "> &STDOUT" ) or die "Can't open stderr";
# print STDOUT getcwd() ."\n";
# print "$0 " . join( " ", @ARGV ) . "\n";;

sub usage( )
{
    print
	"usage: $0 [options] <update-command> <file>\n" .
	"  --period <sec>  specify update period (default 60 seconds)\n".
	"  --max-run <sec> specify maximum run time (default: none)\n".
	"  --delay <sec>   delay before first publish (default: 5 seconds)\n" .
	"  --dir <dir>     specify directory to create temp ad files in\n" .
	"  -k|--keep       Keep temp ad files (for debugging)\n" .
	"  -f              run in foreground (ignored; $0 always does)\n" .
	"  -l|--log <file> specify file to log to\n" .
	"  -n|--no-exec    no execute / test mode\n" .
	"  -v|--verbose    increase verbose level\n" .
	"  -t|--test       print to stdout / stderr\n" .
	"  --valgrind      run test under valgrind\n" .
	"  -q|--quiet      cancel debug & verbose\n" .
	"  -h|--help       this help\n";
}

my %settings =
(
 period			=> 60,
 delay			=> 5,
 keep			=> 0,

 done			=> 0,

 valgrind		=> 0,

 verbose		=> 0,
 execute		=> 1,
 test			=> 0,
 );
my $skip = 0;
foreach my $argno ( 0 .. $#ARGV ) {
	if ( $skip ) {
		$skip--;
		next;
	}
	my $arg  = $ARGV[$argno];
	my $arg1 = ($argno+1 > $#ARGV) ? undef : $ARGV[$argno+1];

	if ( not $arg =~ /^-/ ) {
		if    ( !exists $settings{command} ) {
			$settings{command} = $arg;
		}
		elsif (  !exists $settings{infile} ) {
			$settings{infile} = $arg;
		}
		else {
			print STDERR "Unknown argument $arg\n";
			usage( );
			exit(1);
		}
	}
    elsif ( $arg eq "-n"  or  $arg eq "--no-exec" ) {
		$settings{execute} = 0;
    }
    elsif ( $arg eq "--valgrind" ) {
		$settings{valgrind} = 1;
    }
	elsif ( $arg eq "--period"  and  defined($arg1)  ) {
		$settings{period} = int($arg1);
		$skip = 1;
	}
	elsif ( $arg eq "--delay"  and  defined($arg1)  ) {
		$settings{delay} = int($arg1);
		$skip = 1;
	}
	elsif ( $arg eq "--max-run"  and  defined($arg1)  ) {
		$settings{max_run} = int($arg1);
		$skip = 1;
	}
	elsif ( $arg eq "--dir"  and  defined($arg1)  ) {
		$settings{dir} = $arg1;
		$skip = 1;
	}
	elsif ( $arg eq "--log"  and  defined($arg1)  ) {
		$settings{log} = $arg1;
	}
    elsif ( $arg eq "-k"  or  $arg eq "--keep" ) {
		$settings{keep} = 1;
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
    elsif ( $arg eq "-t"  or  $arg eq "--test" ) {
		$settings{test} = 1;
    }
    elsif ( $arg eq "-h"  or  $arg eq "--help" ) {
		usage( );
		exit(0);
    }
    else {
		print STDERR "Unknown argument $arg\n";
		usage( );
		exit(1);
    }
}
if ( !exists $settings{command} ) {
	print STDERR "No command specified\n";
	usage( );
	exit(1);
}
if ( !exists $settings{infile} ) {
	print STDERR "No input file specified\n";
	usage( );
	exit(1);
}

# Set the directory to use
if ( !exists $settings{dir} ) {
	my @all = split( /\//, $settings{infile} );
	if ( $#all == 1 ) {
		$settings{dir} = $all[0];
	}
	else {
		pop(@all);
		$settings{dir} = join( "/", @all );
	}
	print "Dir set to " . $settings{dir} . "\n";
}

# Setup logging
if ( !exists $settings{log} ) {
	$settings{log} = `condor_config_val ADVERTISER_LOG`;
	chomp $settings{log};
	print "New log: '".$settings{log}."'\n";
}
if ( !$settings{test} ) {
	my $log = $settings{log};
	open( STDOUT, ">$log" );
	open( STDERR, ">&STDOUT" );
}

my @valgrind = ( "valgrind", "--tool=memcheck", "--num-callers=24",
				 "-v", "-v", "--leak-check=full" );

# Read in the original ad
my %ad;
open( AD, $settings{infile} ) or
	die "Can't read ad file '".$settings{infile}."'";
while( <AD> ) {
	chomp;
	if ( /(\w+) = (.*)/ ) {
		$ad{$1} = $2;
	}
	else {
		die "Can't parse ad line $.: '$_'";
	}
}
close( AD );

# Some simple ad manipulations
if ( !exists $ad{DaemonStartTime} ) {
	$ad{DaemonStartTime} = time();
}
my $sequence = 1;
if ( exists $ad{UpdateSequenceNumber} ) {
	$sequence = int($ad{UpdateSequenceNumber});
}

# Set an alarm
sub handler()
{
	print "Caught signal; shutting down\n";
	$settings{done} = 1;
}
$SIG{TERM} = \&handler;
$SIG{INT} = \&handler;
$SIG{QUIT} = \&handler;
if ( exists $settings{max_run} ) {
	$SIG{ALRM} = \&handler;
	alarm( $settings{max_run} );
}

# Delay before first publish
if ( $settings{delay} ) {
	sleep( $settings{delay} );
}

# Now, got into publishing mode
while( ! $settings{done} ) {
	print "loop\n";
	$ad{UpdateSequenceNumber} = $sequence++;
	my ($fh, $filename) = tempfile( DIR => $settings{dir} );
	foreach my $attr ( keys(%ad) ) {
		my $value = $ad{$attr};
		if ( 1 ) {
			print $fh "$attr = $value\n";
		}
		elsif ( $value eq "TRUE" ) {
			print $fh "$attr = TRUE\n";
		}
		elsif ( $value eq "FALSE" ) {
			print $fh "$attr = FALSE\n";
		}
		elsif ( $value =~ /^\d+$/ ) {
			print $fh "$attr = $value\n";
		}
		elsif ( $value =~ /^\d+\.\d+$/ ) {
			print $fh "$attr = $value\n";
		}
		else {
			print $fh "$attr = \"$value\"\n";
		}
	}
	close( $fh );

	if ( $settings{verbose} >= 1 ) {
		print "Temp file: $filename\n";
		if ( $settings{verbose} > 1 ) {
			system( "cat $filename" );
		}
	}

	my $cmd = "";
	if ( $settings{valgrind} ) {
		$cmd .= join(" ", @valgrind );
		$cmd .= " --log-file=".$settings{dir}."/advertise.valgrind ";
	}
	$cmd .= "condor_advertise ".$settings{command}." ".$filename;
	print "Running: $cmd\n";
	if ( $settings{execute} ) {
		system( $cmd );
	}

	if ( not $settings{keep} ) {
		unlink( $filename );
	}

	sleep( $settings{period} );
}


### Local Variables: ***
### mode:perl ***
### tab-width: 4  ***
### End: ***
