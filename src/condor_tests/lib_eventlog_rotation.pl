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

my $testname = 'lib_eventlog_rotation - runs eventlog rotation tests';
my $testbin = "../testbin_dir";
my %programs = ( reader		=> "$testbin/test_log_reader",
		 reader_state	=> "$testbin/test_log_reader_state",
		 writer		=> "$testbin/test_log_writer", );

my $event_size = ( 100000 / 1160 );
my @tests =
    (

     # Default test
     {
	 name => "defaults",
	 config => {
	     "EVENT_LOG"		=> "EventLog",
	     #"EVENT_LOG_COUNT_EVENTS"	=> "FALSE",
	     #"ENABLE_USERLOG_LOCKING"	=> "TRUE",
	     #"EVENT_LOG_MAX_ROTATIONS"	=> 1,
	     #"EVENT_LOG_MAX_SIZE"	=> -1,
	     #"MAX_EVENT_LOG"		=> 1000000,
	 },
	 loops				=> 1,
	 writer => {
	 },
	 reader => {
	 },
     },
     {
	 name		=> "no_rotations_1",
	 config		=> {
	     "EVENT_LOG"		=> "EventLog",
	     "EVENT_LOG_COUNT_EVENTS"	=> "FALSE",
	     "ENABLE_USERLOG_LOCKING"	=> "TRUE",
	     #"EVENT_LOG_MAX_ROTATIONS"	=> 0,
	     "EVENT_LOG_MAX_SIZE"	=> 0,
	     #"MAX_EVENT_LOG"		=> 0,
	 },
	 loops				=> 1,
	 writer => {
	 },
	 reader => {
	 },
     },

     {
	 name		=> "no_rotations_2",
	 config		=> {
	     "EVENT_LOG"		=> "EventLog",
	     "EVENT_LOG_COUNT_EVENTS"	=> "FALSE",
	     "ENABLE_USERLOG_LOCKING"	=> "TRUE",
	     #"EVENT_LOG_MAX_ROTATIONS"	=> 0,
	     "EVENT_LOG_MAX_SIZE"	=> 0,
	     #"MAX_EVENT_LOG"		=> 0,
	 },
	 loops				=> 1,
	 writer => {
	 },
	 reader => {
	 },
     },
     {
	 name		=> "no_rotations_3",
	 config		=> {
	     "EVENT_LOG"		=> "EventLog",
	     "EVENT_LOG_COUNT_EVENTS"	=> "FALSE",
	     "ENABLE_USERLOG_LOCKING"	=> "TRUE",
	     #"EVENT_LOG_MAX_ROTATIONS"	=> 0,
	     #"EVENT_LOG_MAX_SIZE"	=> 0,
	     "MAX_EVENT_LOG"		=> 0,
	 },
	 loops				=> 1,
	 writer => {
	 },
	 reader => {
	 },
     },

     {
	 name		=> "w_old_rotations",
	 config		=> {
	     "EVENT_LOG"		=> "EventLog",
	     "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
	     "ENABLE_USERLOG_LOCKING"	=> "TRUE",
	     "EVENT_LOG_MAX_ROTATIONS"	=> 1,
	     "EVENT_LOG_MAX_SIZE"	=> 10000,
	     #"MAX_EVENT_LOG"		=> -1,
	 },
	 loops				=> 1,
	 writer => {
	 },
	 reader => {
	 },
     },

     {
	 name		=> "w_2_rotations",
	 config		=> {
	     "EVENT_LOG"		=> "EventLog",
	     "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
	     "ENABLE_USERLOG_LOCKING"	=> "TRUE",
	     "EVENT_LOG_MAX_ROTATIONS"	=> 2,
	     "EVENT_LOG_MAX_SIZE"	=> 10000,
	     #"MAX_EVENT_LOG"		=> -1,
	 },
	 loops				=> 1,
	 writer => {
	 },
	 reader => {
	 },
     },

     {
	 name		=> "w_5_rotations",
	 config		=> {
	     "EVENT_LOG"		=> "EventLog",
	     "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
	     "ENABLE_USERLOG_LOCKING"	=> "TRUE",
	     "EVENT_LOG_MAX_ROTATIONS"	=> 5,
	     "EVENT_LOG_MAX_SIZE"	=> 10000,
	     #"MAX_EVENT_LOG"		=> -1,
	 },
	 loops				=> 1,
	 writer => {
	 },
	 reader => {
	 },
     },

     {	
	 name		=> "w_20_rotations",
	 config		=> {
	     "EVENT_LOG"		=> "EventLog",
	     "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
	     "ENABLE_USERLOG_LOCKING"	=> "TRUE",
	     "EVENT_LOG_MAX_ROTATIONS"	=> 20,
	     "EVENT_LOG_MAX_SIZE"	=> 10000,
	     #"MAX_EVENT_LOG"		=> -1,
	 },
	 loops				=> 1,
	 writer => {
	 },
	 reader => {
	 },
     }
);

sub usage( )
{
    print
	"usage: $0 [options] test-name\n" .
	"  -l|--list     list names of tests\n" .
	"  -f|--force    force overwrite of test directory\n" .
	"  -t|-n|--test  test mode (no execute)\n" .
	"  -v|--verbose  increase verbose level\n" .
	"  -h|--help     this help\n";
}

my %settings =
(
 verbose	=> 0,
 test		=> 0,
 force		=> 0,
 );
foreach my $arg ( @ARGV ) {
    if ( $arg eq "-f"  or  $arg eq "--force" ) {
	$settings{force} = 1;
    }
    elsif ( $arg eq "-l"  or  $arg eq "--list" ) {
	foreach my $test ( @tests ) {
	    print "  " . $test->{name} . "\n";
	}
	exit(0);
    }
    elsif ( $arg eq "-t"  or  $arg eq "-n"  or  $arg eq "--test" ) {
	$settings{test} = 1;
    }
    elsif ( $arg eq "-v"  or  $arg eq "--verbose" ) {
	$settings{verbose}++;
    }
    elsif ( $arg eq "-h"  or  $arg eq "--help" ) {
	usage( );
	exit(0);
    }
    elsif ( !($arg =~ /^-/)  and !exists($settings{name}) ) {
	$settings{name} = $arg;
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

my $test;
foreach my $t ( @tests )
{
    if ( $t->{name} eq $settings{name} ) {
	$test = $t;
	last;
    }
}

if ( ! defined($test) ) {
    die "Unknown test name: $test";
}

my $dir = getcwd() . "/test-eventlog_rotation-" . $settings{name};
if ( -d $dir  and  $settings{force} ) {
    system( "/bin/rm", "-fr", $dir );
}
if ( -d $dir ) {
    die "Test directory $dir already exists! (try --force?)";
}
mkdir( $dir ) or die "Can't create directory $dir";
print "writing to $dir\n" if ( $settings{verbose} );

my $config = "$dir/condor_config";
open( CONFIG, ">$config" ) or die "Can't write to config file $config";
foreach my $param ( keys(%{$test->{config}}) ) {
    my $value = $test->{config}{$param};
    if ( $value eq "TRUE" ) {
	print CONFIG "$param = TRUE\n";
    }
    elsif ( $value eq "FALSE" ) {
	print CONFIG "$param = FALSE\n";
    }
    elsif ( $param eq "EVENT_LOG" ) {
	print CONFIG "$param = $dir/$value\n";
    }
    else {
	print CONFIG "$param = $value\n";
    }
}
close( CONFIG );
$ENV{"CONDOR_CONFIG"} = $config;

# Basic calculations
my $default_max_size = 1000000;
my $max_size = $default_max_size;
my $max_rotations = 1;
if ( exists $test->{config}{"EVENT_LOG_MAX_SIZE"} ) {
    $max_size = $test->{config}{"EVENT_LOG_MAX_SIZE"};
}
elsif ( exists $test->{config}{"MAX_EVENT_LOG"} ) {
    $max_size = $test->{config}{"MAX_EVENT_LOG"};
}
if ( exists $test->{config}{"EVENT_LOG_MAX_ROTATIONS"} ) {
    $max_rotations = $test->{config}{"EVENT_LOG_MAX_ROTATIONS"};
}
elsif ( $max_size == 0 ) {
    $max_rotations = 0;
    $max_size = $default_max_size;
}
my $total_files = $max_rotations + 1;
my $total_loops = $test->{loops};
my $loop_events =
    int(1.25 * $total_files * $max_size / ($event_size * $total_loops) );

# Calculate num exec / procs
if ( !exists $test->{writer}{"--num-exec"} ) {

    my $num_procs = 1;
    if ( exists( $test->{writer}{"--num-procs"} ) ) {
	$num_procs = $test->{writer}{"--num-procs"};
    }
    else {
	$test->{writer}{"--num-procs"} = $num_procs;
    }
    $test->{writer}{"--num-exec"} = int( $loop_events / $num_procs );
}
if ( !exists $test->{writer}{"--sleep"} ) {
    $test->{writer}{"--no-sleep"} = undef;
}

my $loop_files	= int( $total_files / $total_loops );
my $total_events = $loop_events * $total_loops;

my @writer_args = ( $programs{writer} );
foreach my $t ( keys(%{$test->{writer}}) ) {
    if ( $t =~ /^-/ ) {
	my $value = $test->{writer}{$t};
	push( @writer_args, $t );
	if ( defined($value) ) {
	    push( @writer_args, $test->{writer}{$t} );
	}
    }
}
push( @writer_args, "--verbosity" );
push( @writer_args, "INFO" );
push( @writer_args, "--debug" );
push( @writer_args, "D_FULLDEBUG" );
if ( exists $test->{writer}{file} ) {
    push( @writer_args, $test->{writer}{file} )
}
else {
    push( @writer_args, "/dev/null" )
}


system("date");
my $errors = 0;
my $sequence = 0;
my $num_events;
foreach my $loop ( 1 .. $test->{loops} ) {
    print join( " ", @writer_args ) . "\n";

    my $events = 0;
    if ( ! $settings{test} ) {
	my $prev_sequence = $sequence;
	my $cmd = join(" ", @writer_args );
	open( WRITER, "$cmd|" ) or die "Can't run $cmd";
	while( <WRITER> ) {
	    print if ( $settings{verbose} );
	    chomp;
	    if ( /wrote (\d+) events/ ) {
		$events = $1;
	    }
	    elsif ( /global sequence (\d+)/ ) {
		$sequence = $1;
	    }
	}
	close( WRITER );
	$num_events += $events;
	my $files = 1 + $sequence - $prev_sequence;
	if ( $events < $loop_events ) {
	    printf( STDERR
		    "WARNING: too few events written in loop: %d < %d\n",
		    $events, $loop_events );
	    $errors++;
	}
	if ( $files < $loop_files ) {
	    printf( STDERR
		    "WARNING: too few files written in loop: %d < %d\n",
		    $files, $loop_files );
	    $errors++;
	}
    }
}

if ( $num_events < $total_events ) {
    printf( STDERR
	    "WARNING: too few events written: %d < %d\n",
	    $num_events, $total_events );
    $errors++;
}
if ( ($sequence+1) < $total_files ) {
    printf( STDERR
	    "WARNING: too few files written: %d < %d\n",
	    ($sequence+1), $total_files );
    $errors++;
}

if ( $errors ) {
    my $cmd;
    print "\nls:\n";
    $cmd = "/bin/ls -l $dir";
    system( $cmd );
    print "\nwc:\n";
    $cmd = "wc $dir/*";
    system( $cmd );
    print "\nhead:\n";
    $cmd = "head -2 $dir/EventLog*";
    system( $cmd );
    print "\nconfig:\n";
    $cmd = "cat $config";
    system( $cmd );
}

exit( $errors == 0 ? 0 : 1 );
