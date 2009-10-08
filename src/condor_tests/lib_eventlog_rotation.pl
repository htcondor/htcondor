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
use Config;

# Initialize signals
defined $Config{sig_name} || die "No signal names";
my %signos;
my @signames = split( ' ', $Config{sig_name} );
foreach my $i ( 0 .. $#signames ) {
    $signos{$signames[$i]} = $i;
}
exists $signos{CONT} || die "No CONT signal";
exists $signos{TERM} || die "No TERM signal";
exists $signos{KILL} || die "No KILL signal";

my $version = "1.1.0";
my $testdesc =  'lib_eventlog_rotation - runs eventlog rotation tests';
my $testname = "lib_eventlog_rotation";
my $testbin = "../testbin_dir";
my %programs = ( reader			=> "$testbin/test_log_reader",
				 reader_state	=> "$testbin/test_log_reader_state",
				 writer			=> "$testbin/test_log_writer", );

my $DefaultMaxEventLog = 1000000;
my $EventSize = ( 100045 / 1177 );
my @tests =
    (

     # Test with default configuration
     {
		 name => "defaults",
		 config => {
			 "EVENT_LOG"				=> "EventLog",
			 #"EVENT_LOG_COUNT_EVENTS"	=> "FALSE",
			 #"EVENT_LOG_MAX_ROTATIONS"	=> 1,
			 #"EVENT_LOG_MAX_SIZE"		=> -1,
			 #"MAX_EVENT_LOG"			=> 1000000,
			 #"ENABLE_USERLOG_LOCKING"	=> "TRUE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 writer => {
		 },
     },

     # Tests with rotations disabled
     {
		 name		=> "no_rotations_1",
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "FALSE",
			 #"EVENT_LOG_MAX_ROTATIONS"	=> 0,
			 "EVENT_LOG_MAX_SIZE"		=> 0,
			 #"MAX_EVENT_LOG"			=> 0,
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 writer => {
			 "--max-rotations"			=> 1,
		 },
     },
     {
		 name		=> "no_rotations_2",
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "FALSE",
			 #"EVENT_LOG_MAX_ROTATIONS"	=> 0,
			 "EVENT_LOG_MAX_SIZE"		=> 0,
			 #"MAX_EVENT_LOG"			=> 0,
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 writer => {
			 "--max-rotations"			=> 1,
		 },
     },
     {
		 name		=> "no_rotations_3",
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "FALSE",
			 #"EVENT_LOG_MAX_ROTATIONS"	=> 0,
			 #"EVENT_LOG_MAX_SIZE"		=> 0,
			 "MAX_EVENT_LOG"			=> 0,
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 writer => {
			 "--max-rotations"			=> 1,
		 },
     },

     # Tests with ".old" rotations
     {
		 name		=> "w_old_rotations",
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
			 "EVENT_LOG_MAX_ROTATIONS"	=> 1,
			 "EVENT_LOG_MAX_SIZE"		=> 10000,
			 #"MAX_EVENT_LOG"			=> -1,
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 auto				=> 1,
		 writer => {
		 },
     },

     # Tests with 2 rotations (.1, .2)
     {
		 name		=> "w_2_rotations",
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
			 "EVENT_LOG_MAX_ROTATIONS"	=> 2,
			 "EVENT_LOG_MAX_SIZE"		=> 10000,
			 #"MAX_EVENT_LOG"			=> -1,
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 auto				=> 1,
		 writer => {
		 },
     },

     {
		 name		=> "w_5_rotations",
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
			 "ENABLE_USERLOG_LOCKING"	=> "TRUE",
			 "EVENT_LOG_MAX_ROTATIONS"	=> 5,
			 "EVENT_LOG_MAX_SIZE"		=> 10000,
			 #"MAX_EVENT_LOG"			=> -1,
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 auto				=> 1,
		 writer => {
		 },
     },

     {	
		 name		=> "w_20_rotations",
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
			 "ENABLE_USERLOG_LOCKING"	=> "TRUE",
			 "EVENT_LOG_MAX_ROTATIONS"	=> 20,
			 "EVENT_LOG_MAX_SIZE"		=> 10000,
			 #"MAX_EVENT_LOG"			=> -1,
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 auto				=> 1,
		 writer => {
		 },
     },

	 # Test with force close enabled
     {
		 name => "close_log",
		 config => {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
			 "EVENT_LOG_MAX_ROTATIONS"	=> 2,
			 "EVENT_LOG_MAX_SIZE"		=> 10000,
			 "EVENT_LOG_FORCE_CLOSE"	=> "TRUE",
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 auto				=> 1,
		 writer => {
		 },
     },

	 # ##########################
	 # Reader tests start here
	 # ##########################

     {
		 name		=> "reader_simple",
		 config		=> {
			 "USER_LOG_FSYNC"			=> "FALSE",
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
			 "EVENT_LOG_MAX_ROTATIONS"	=> 0,
			 #"EVENT_LOG_MAX_SIZE"		=> 10000,
			 #"MAX_EVENT_LOG"			=> -1,
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 writer => {
		 },
		 reader => {
		 },
     },

     {
		 name		=> "reader_old_rotations",
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
			 "EVENT_LOG_MAX_ROTATIONS"	=> 1,
			 "EVENT_LOG_MAX_SIZE"		=> 10000,
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 auto						=> 1,
		 writer => {
		 },
		 reader => {
			 persist				=> 1,
		 },
     },

     {
		 name		=> "reader_2_rotations",
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
			 "EVENT_LOG_MAX_ROTATIONS"	=> 2,
			 "EVENT_LOG_MAX_SIZE"		=> 10000,
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 auto						=> 1,
		 writer => {
		 },
		 reader => {
			 persist				=> 1,
		 },
     },

     {
		 name		=> "reader_20_rotations",
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
			 "EVENT_LOG_MAX_ROTATIONS"	=> 20,
			 "EVENT_LOG_MAX_SIZE"		=> 10000,
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 auto						=> 1,
		 writer => {
		 },
		 reader => {
			 persist				=> 1,
		 },
     },

     {
		 name		=> "reader_missed_1",
		 loops		=> 5,
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
			 "EVENT_LOG_MAX_ROTATIONS"	=> 2,
			 "EVENT_LOG_MAX_SIZE"		=> 10000,
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 auto						=> 1,
		 writer => {
		 },
		 reader => {
			 persist				=> 1,
			 loops					=> {
				 1 => {},
				 5 => { "missed_ok" => 1, },
			 },
		 },
     },

     {
		 name		=> "reader_missed_2",
		 loops		=> 5,
		 config		=> {
			 "EVENT_LOG"				=> "EventLog",
			 "EVENT_LOG_COUNT_EVENTS"	=> "TRUE",
			 "EVENT_LOG_MAX_ROTATIONS"	=> 2,
			 "EVENT_LOG_MAX_SIZE"		=> 10000,
			 "ENABLE_USERLOG_LOCKING"	=> "FALSE",
			 "ENABLE_USERLOG_FSYNC"		=> "FALSE",
		 },
		 auto						=> 1,
		 writer => {
		 },
		 reader => {
			 persist				=> 1,
			 loops					=> {
				 1 => { "stop" => 1, },
				 5 => { "continue" => 1, "missed_ok" => 1 },
			 },
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
		"  -p|--pid      Append PID to test directory\n" .
		"  -N|--no-pid   Don't append PID to test directory (default)\n" .
		"  -d|--debug    enable D_FULLDEBUG debugging\n" .
		"  --dump_state  enable state dumping in reader\n" .
		"  --loops=<n>   Override # of loops in test\n" .
		"  -v|--verbose  increase verbose level\n" .
		"  -s|--stop     stop after errors\n" .
		"  --vg-writer   run writer under valgrind\n" .
		"  --vg-reader   run reader under valgrind\n" .
		"  --strace-writer run writer under strace\n" .
		"  --strace-reader run reader under strace\n" .
		"  --strace      strace myself\n" .
		"  --no-strace   don't strace myself\n" .
		"  --version     print version information and exit\n" .
		"  -q|--quiet    cancel debug & verbose\n" .
		"  -h|--help     this help\n";
}

sub RunWriter( $$$$$$ );
sub RunReader( $$$$$ );
sub CopyCore( $$$ );
sub ReadEventlogs( $$$ );
sub ProcessEventlogs( $$ );
sub CheckWriterOutput( $$$$ );
sub CheckReaderOutput( $$$$ );
sub GatherData( $$ );
sub FinalCheck( );

my %settings =
(
 use_pid			=> 0,
 verbose			=> 0,
 debug				=> 0,
 reader_dump_state	=> 0,
 execute			=> 1,
 stop				=> 0,
 strace				=> 0,
 force				=> 0,
 strace_writer		=> 0,
 strace_reader		=> 0,
 valgrind_writer	=> 0,
 valgrind_reader	=> 0,
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
    elsif ( $arg eq "--dump-state" ) {
		$settings{reader_dump_state} = 1;
    }
    elsif ( $arg eq "--strace" ) {
		$settings{strace} = 1;
    }
    elsif ( $arg eq "--no-strace" ) {
		$settings{strace} = 0;
    }
    elsif ( $arg eq "--strace-writer" ) {
		$settings{strace_writer} = 1;
    }
    elsif ( $arg eq "--strace-reader" ) {
		$settings{strace_reader} = 1;
    }
    elsif ( $arg eq "--vg-writer" ) {
		$settings{valgrind_writer} = 1;
    }
    elsif ( $arg eq "--vg-reader" ) {
		$settings{valgrind_reader} = 1;
    }
    elsif ( $arg =~ /^--loops=(\d+)$/ ) {
		$settings{loops} = $1;
    }
    elsif ( $arg eq "-v"  or  $arg eq "--verbose" ) {
		$settings{verbose}++;
    }
    elsif ( $arg eq "--version" ) {
		printf "Eventlog Rotation Test version %s\n", $version;
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
    elsif ( !($arg =~ /^-/)  and !exists($settings{name}) ) {
		if ( $arg =~ /(\w+?)(?:_(\d+)_loops)/ ) {
			$settings{name} = $1;
			$settings{loops} = $2;
		}
		else {
			$settings{name} = $arg;
		}
    }
    else {
		print STDERR "Unknown argument $arg\n";
		usage( );
		exit(1);
    }
}
$#ARGV = -1;
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
    die "Unknown test name: $settings{name}";
}

my $dir = "test-eventlog_rotation-" . $settings{name};
if ( exists $settings{loops} ) {
    $dir .= "_".$settings{loops}."_loops";
}
if ( $settings{use_pid} ) {
	$dir .= ".$$";
}
my $fulldir = getcwd() . "/$dir";
if ( -d $dir  and  $settings{force} ) {
    system( "/bin/rm", "-fr", $dir );
}
if ( -d $dir ) {
    die "Test directory $dir already exists! (try --force?)";
}
mkdir( $dir ) or die "Can't create directory $dir";
print "writing to $dir\n" if ( $settings{verbose} );

my $config = "$fulldir/condor_config";
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
		print CONFIG "$param = $fulldir/$value\n";
    }
    else {
		print CONFIG "$param = $value\n";
    }
}
print CONFIG "TEST_LOG_READER_STATE_LOG = $fulldir/state.log\n";
print CONFIG "TEST_LOG_READER_LOG = $fulldir/reader.log\n";
print CONFIG "TEST_LOG_WRITER_LOG = $fulldir/writer.log\n";
close( CONFIG );
$ENV{"CONDOR_CONFIG"} = $config;


# ################################
# startup strace
# ################################
my $strace_pid = 0;
if ( $settings{strace} ) {
	print "Starting strace (my pid is $$)\n";
	$strace_pid = fork();
	if ( $strace_pid == 0 ) {
		my $pid = getppid();
		my $strace = "strace -t -e trace=process -o $dir/strace.out -p $pid";
		exec( "$strace" ) or exit(1);
	}
	else {
		sleep(1);		# let strace start
	}
}

# ######################
# Basic calculations
# ######################
my %params;

# Max eventlog setting
$params{max_size} = $DefaultMaxEventLog;
if ( exists $test->{config}{"EVENT_LOG_MAX_SIZE"} ) {
    $params{max_size} = $test->{config}{"EVENT_LOG_MAX_SIZE"};
}
elsif ( exists $test->{config}{"MAX_EVENT_LOG"} ) {
    $params{max_size} = $test->{config}{"MAX_EVENT_LOG"};
}

# Max rotation setting
$params{max_rotations} = 1;
if ( exists $test->{config}{"EVENT_LOG_MAX_ROTATIONS"} ) {
    $params{max_rotations} = $test->{config}{"EVENT_LOG_MAX_ROTATIONS"};
}
elsif ( $params{max_size} == 0 ) {
    $params{max_rotations} = 0;
    $params{max_size} = $DefaultMaxEventLog;
}

# Stop at max rotation specified?
if ( exists $test->{auto} ) {
    $test->{writer}{"--max-user"} = 0;
    $test->{writer}{"--max-global"} = 0;
}

# Number of event log files generated
$params{files} = $params{max_rotations} + 1;

# Number of loops
if ( exists $settings{loops} ) {
    $params{loops} = $settings{loops};
    $test->{loops} = $settings{loops};
}
elsif ( exists $test->{loops} ) {
    $params{loops} = $test->{loops};
}
else {
    $params{loops} = 1;
    $test->{loops} = 1;
}

# User log size
if ( !exists $test->{user_size_mult} ) {
	$test->{user_size_mult} = $params{files} + 0.25;
}
if ( !exists( $test->{user_size} ) ) {
	$test->{user_size} = $params{max_size} * $test->{user_size_mult};
}
if ( $test->{user_size} > 0 ) {
    $test->{writer}{"--max-user"} = $test->{user_size};
}

# Event log size
if ( !exists $test->{global_size_mult} ) {
	$test->{global_size_mult} = 1.25;
}
if ( !exists( $test->{global_size} ) ) {
	$test->{global_size} = $params{max_size} * $test->{global_size_mult};
}
if ( $test->{global_size} > 0 ) {
    $test->{writer}{"--max-global"} = $test->{global_size};
}

# Total size
$params{total_size} = ( $test->{global_size} * $params{files} );
if ( $params{total_size} > $test->{user_size} ) {
	$params{total_size} = $test->{user_size};
}

# Subtract off some fixed amounts for header, submit & terminate event(s)
$params{total_esize}  = $params{total_size};
$params{total_esize} -= ( 256 * $params{files} );		# header events
$params{total_esize} -= ( 80 * $params{loops} );		# submit events
$params{total_esize} -= ( 450 * ($params{loops}-1) );	# terminate events

# This depends on the number of loops per file
$params{lpf}         = ( $params{loops} / $params{files} );
$params{max_esize}   = $params{max_size};
$params{max_esize}   -= 256;							# header events
$params{max_esize}   -= ( 80 * $params{lpf} );			# submit events
$params{max_esize}   -= ( 450 * ($params{lpf}-1) );		# terminate events

# Auto max rotations / loop
if ( exists $test->{auto} ) {
	my $fpl = int( ($params{files} / $params{loops}) + 0.99999 );
    $test->{writer}{"--max-rotations"} = $fpl;
}

# Max # of rotations
if ( !exists $test->{max_rotations} ) {
	$test->{max_rotations} = 0;
}
if ( $test->{max_rotations} > 0 ) {
    $test->{writer}{"--max-rotations"} =
		int( 0.9999 + ($test->{max_rotations} / $test->{loops}) );
	$params{loop_events}  = int( $params{max_esize} / $EventSize );
	$params{total_events} = int( $params{total_esize} / $EventSize );
}
else {
	$params{loop_events}  = int( $params{max_esize} / $EventSize );
	$params{total_events} = $params{loop_events};
}

# Min number of events expected to be written per loop
if ( exists $test->{loop_min_events} ) {
	$params{loop_min_events} = $test->{loop_min_events};
}
else {
	$params{loop_min_events} = 0.9 * $params{loop_events};
}

# Min number of events expected to be written total
if ( exists $test->{min_events} ) {
	$params{total_min_events} = $test->{min_events}
}
else {
	$params{total_min_events} = 0.9 * $params{total_events};
}

# Sleep time?  Probably --no-sleep
if ( !exists $test->{writer}{"--sleep"} ) {
    $test->{writer}{"--no-sleep"} = undef;
}

# Expected results
my %expect =
	(
	 final_mins => {
		 num_files		=> $params{files},
		 sequence		=> $params{files},
		 file_size		=> $params{max_size},
		 num_events		=> $params{total_min_events},
	 },
	 maxs => {
		 file_size		=> $params{max_size} + 1024,
		 total_size		=> ($params{max_size} + 1024) * $params{files},
	 },
	 loop_mins => {
		 num_files		=> int($params{files} / $params{loops}),
		 sequence		=> int($params{files} / $params{loops}),
		 file_size		=> $params{max_size} / $params{loops},
		 num_events		=> $params{loop_min_events},
	 },
	 cur_mins => {
		 num_files		=> 0,
		 sequence		=> 0,
		 file_size		=> 0,
		 toal_size		=> 0,
		 num_events		=> 0,
	 },
	 );


# Generate the writer command line
my @writer_args = ( $programs{writer} );
foreach my $t ( keys(%{$test->{writer}}) ) {
    if ( $t =~ /^-/ ) {
		my $value = $test->{writer}{$t};
		push( @writer_args, $t );
		if ( defined($value) ) {
			push( @writer_args, $value );
		}
    }
}

push( @writer_args, "--verbosity" );
push( @writer_args, "INFO" );
foreach my $n ( 2 .. $settings{verbose} ) {
    push( @writer_args, "-v" );
}
if ( $settings{debug} ) {
    push( @writer_args, "--debug" );
    push( @writer_args, "D_FULLDEBUG" );
}

# Userlog file
if ( !exists $test->{writer}{file} ) {
	$test->{writer}{file} = "$dir/userlog";
}
push( @writer_args, $test->{writer}{file} );

# Generate the reader command line
my @reader_args;
if ( exists $test->{reader} ) {
	$settings{run_reader} = 1;
    push( @reader_args, $programs{reader} );
    foreach my $t ( keys(%{$test->{reader}}) ) {
		if ( $t =~ /^-/ ) {
			my $value = $test->{writer}{$t};
			push( @reader_args, $t );
			if ( defined($value) ) {
				push( @reader_args, $test->{reader}{$t} );
			}
		}
    }
    push( @reader_args, "--verbosity" );
    push( @reader_args, "ALL" );
    if ( $settings{debug} ) {
		push( @reader_args, "--debug" );
		push( @reader_args, "D_FULLDEBUG" );
    }
    push( @reader_args, "--eventlog" );
    push( @reader_args, "--exit" );
    push( @reader_args, "--no-term" );

    if ( exists $test->{reader}{persist} ) {
		push( @reader_args, "--persist" );
		push( @reader_args, "$dir/reader.state" );
    }
	if ( $settings{reader_dump_state} ) {
		push( @reader_args, "--dump-state" );
	}
}
else {
	$settings{run_reader} = 0;
}


my @valgrind = ( "valgrind", "--tool=memcheck", "--num-callers=24",
				 "-v", "-v", "--leak-check=full", "--track-fds=yes" );
my @strace = ( "strace", "-e trace=file" );

my $start = time( );
my $timestr = localtime($start);
printf "** Eventlog rotation test v%s starting test %s (%d loops) @ %s **\n",
	$version, $settings{name}, $test->{loops}, $timestr;
if ( $settings{verbose} ) {
    print "Using directory $dir\n";
}
my $total_errors = 0;

# Total writer out
my %totals = ( sequence => 0,
			   num_events => 0,
			   max_rot => 0,
			   file_size => 0,

			   events_lost => 0,
			   num_events_lost => 0,

			   writer_events => 0,
			   writer_sequence => 0,

			   reader_events => 0,
			   missed_events => 0,
			   );

# Actual from previous loop
my %previous;
foreach my $k ( keys(%totals) ) {
    $previous{$k} = 0;
}

my %reader_state;
foreach my $loop ( 1 .. $test->{loops} )
{
	my $errors;	# Temp error count

	my $final = ( $loop == $test->{loops} );
	$timestr = localtime();
	printf( "\n** Starting loop $loop %s@ %s **\n",
			$final ? "(final) " : "", $timestr );

    foreach my $k ( keys(%{$expect{loop_mins}}) ) {
		$expect{cur_mins}{$k} += $expect{loop_mins}{$k};
    }

    # writer output from this loop
    my %new;
    for my $k ( keys(%totals) ) {
		$new{$k} = 0;
    }

    # Run the writer
    my $run;
    $errors += RunWriter( $loop, $final,
						  \%new, \%previous, \%totals, \$run );
    $total_errors += $errors;
    if ( ! $run ) {
		next;
    }
    if ( $errors ) {
		print STDERR "writer failed: aborting test\n";
		last;
    }

	my @files;
    $errors = ReadEventlogs( $dir, \%new, \@files );
	if ( $errors ) {
		print STDERR "errors detected in ReadEventLogs()\n";
		$total_errors += $errors;
	}

    $errors = ProcessEventlogs( \@files, \%new );
	if ( $errors ) {
		print STDERR "errors detected in ProcessEventLogs()\n";
		$total_errors += $errors;
	}

    my %diff;
    foreach my $k ( keys(%previous) ) {
		$diff{$k} = $new{$k} - $previous{$k};
    }
    $errors = CheckWriterOutput( \@files, $loop, \%new, \%diff );
	if ( $errors ) {
		print STDERR "errors detected in CheckWriterOutput()\n";
		$total_errors += $errors;
	}

    # Run the reader
	my $opts = { };
	my $run_reader = $settings{run_reader};
	if ( exists $test->{reader}{loops} ) {
		if ( !exists $test->{reader}{loops}{$loop} ) {
			print "Skipping reading on loop $loop\n";
			$run_reader = 0;
		}
		$opts = $test->{reader}{loops}{$loop};
	}
	if ( $run_reader ) {
		$errors = RunReader( $loop, \%new, \$run, \%reader_state, $opts );
		if ( $errors ) {
			print STDERR "errors detected in RunReader()\n";
			$total_errors += $errors;
		}
		if ( $run ) {
			$errors = CheckReaderOutput( \@files, $loop, \%new, \%diff );
			if ( $errors ) {
				print STDERR "errors detected in CheckReaderOutput()\n";
				$total_errors += $errors;
			}
		}
	}

    foreach my $k ( keys(%previous) ) {
		$totals{$k} += $new{$k};
    }

    if ( $total_errors  or  $settings{verbose}  or  $settings{debug} ) {
		GatherData( sprintf("$dir/loop-%02d.txt",$loop), $total_errors );
    }

    foreach my $k ( keys(%previous) ) {
		$previous{$k} = $new{$k};
    }
    if ( $total_errors  and  $settings{stop} ) {
		last;
    }
}

$timestr = localtime();
printf "\n** End test loops @ %s **\n", $timestr;

if ( $settings{execute} ) {
	$total_errors += FinalCheck( );

	if ( $total_errors  or  ($settings{verbose} > 2)  or  $settings{debug} ) {
		GatherData( "/dev/stdout", $total_errors );
	}
}

my $exit_status = $total_errors == 0 ? 0 : 1;

my $end = time();
$timestr = localtime( $end );
my $duration = $end - $start;
printf "\n** Analysis complete **\n";
printf( "\n** Eventlog rotation test '%s' done @ %s; %ds, status %d **\n",
		$settings{name}, $timestr, $duration, $exit_status );
if ( $strace_pid > 0 ) {
	print "Killing strace\n";
	kill( $signos{TERM}, $strace_pid );
	sleep(1);
}

exit $exit_status;


sub GatherData( $$ )
{
    my $f = shift;
	my $errors = shift;
    if ( !open( OUT, ">$f" ) ) {
		print STDERR "Failed to open log file $f for writing: $!\n";
		open( OUT, ">&STDERR" ) or die "Can't dup standard error";
	}

    print OUT "\n\n** Total: $errors errors detected **\n";

    my $cmd;

    print OUT "\nls:\n";
    $cmd = "/bin/ls -l $dir";
    if ( open( CMD, "$cmd |" ) ) {
		while( <CMD> ) {
			print OUT;
		}
    }

    print OUT "\nwc:\n";
    $cmd = "wc $dir/EventLog*";
    if ( open( CMD, "$cmd |" ) ) {
		while( <CMD> ) {
			print OUT;
		}
    }

    print OUT "\nhead:\n";
    $cmd = "head -1 $dir/EventLog*";
    if ( open( CMD, "$cmd |" ) ) {
		while( <CMD> ) {
			print OUT;
		}
    }

    print OUT "\nconfig:\n";
    $cmd = "cat $config";
    if ( open( CMD, "$cmd |" ) ) {
		while( <CMD> ) {
			print OUT;
		}
    }

    print OUT "\ndirectory:\n";
    print OUT "$dir\n";

    close( OUT );
}

# #######################################
# Run the writer
# #######################################
sub RunWriter( $$$$$$ )
{
    my $loop = shift;
	my $final = shift;
    my $new = shift;
    my $previous = shift;
    my $totals = shift;
    my $run = shift;

    my $errors = 0;

	# Blow away any userlog file
	if ( -f $writer_args[-1] ) {
		print "Removing " . $writer_args[-1] . "\n" if ( $settings{verbose} );
		unlink( $writer_args[-1] );
	}

    my $cmd = "";

    my $vg_out = sprintf( "valgrind-writer-%02d.out", $loop );
    my $vg_full = "$dir/$vg_out";

    if ( $settings{valgrind_writer} ) {
		$cmd .= join( " ", @valgrind ) . " ";
		$cmd .= " --log-file=$vg_full ";
    }
	elsif ( $settings{strace_writer} ) {
		my $strace_out = sprintf( "strace-writer-%02d.out", $loop );
		my $strace_full = "$dir/$strace_out";
		$cmd .= join( " ", @strace ) . " ";
		$cmd .= " -o $strace_full ";
    }

    $cmd .= join(" ", @writer_args );

	# Prevent rotations on the final loop;
	#   Has no effect if --max-rotations is not specified
	if ( $final ) {
		$cmd .= " --max-rotation-stop ";
	}
    print "Running: $cmd\n" if ( $settings{verbose} );

    if ( ! $settings{execute} ) {
		$$run = 0;
		return 0;
    }

    $$run = 1;
    my $out = sprintf( "%s/writer-%02d.out", $dir, $loop );
    my $pid = open( WRITER, "$cmd 2>&1 |" );
	$pid or die "Can't run $cmd";
    open( OUT, ">$out" );
    while( <WRITER> ) {
		print if ( $settings{verbose} > 1 );
		print OUT;
		chomp;
		if ( /wrote (\d+) events/ ) {
			$new->{writer_events} = $1;
		}
		elsif ( /global sequence (\d+)/ ) {
			$new->{writer_sequence} = $1;
			$new->{writer_files} = $1 - $previous->{writer_sequence};
			$totals->{writer_sequence} = 0;	# special case
		}
    }
    close( WRITER );
    close( OUT );

    if ( $? & 127 ) {
		printf "ERROR: writer (PID $$) exited from signal %d\n", ($? & 127);
		$errors++;
    }
    if ( $? & 128 ) {
		print "ERROR: writer (PID $$) dumped core\n";
		CopyCore( $writer_args[0], $pid, $dir );
		$errors++;
    }
    if ( $? >> 8 ) {
		printf "ERROR: writer (PID $$) exited with status %d\n", ($? >> 8);
		$errors++;
    }
    if ( ! $errors and $settings{verbose}) {
		print "writer process exited normally\n";
    }

    if ( $settings{valgrind_writer} ) {
		$errors += CheckValgrind( $vg_out );
    }
    return $errors;
}



# #######################################
# Run the reader
# #######################################
sub RunReader( $$$$$ )
{
    my $loop = shift;
    my $new = shift;
    my $run = shift;
	my $state = shift;
	my $opts = shift;

    my $errors = 0;

	if ( exists $opts->{"continue"} ) {
		if ( (!exists $state->{"pid"}) or ($state->{"pid"} == 0) ) {
			die "Can't continue reader: no PID\n";
		}
		if ( !exists $state->{"pipe"} ) {
			die "Can't continue reader: no pipe\n";
		}
	}

    my $cmd = "";
    my $vg_out = sprintf( "valgrind-reader-%02d.out", $loop );
    my $vg_full = "$dir/$vg_out";
    if ( $settings{valgrind_reader} ) {
		$cmd .= join( " ", @valgrind ) . " ";
		$cmd .= " --log-file=$vg_full ";
    }
	elsif ( $settings{strace_reader} ) {
		my $strace_out = sprintf( "strace-reader-%02d.out", $loop );
		my $strace_full = "$dir/$strace_out";
		$cmd .= join( " ", @strace ) . " ";
		$cmd .= " -o $strace_full ";
    }

    $cmd .= join(" ", @reader_args );
	if ( exists $opts->{"stop"} ) {
		$cmd .= " --stop";
	}
    print "Running: $cmd\n" if ( $settings{verbose} );

    if ( ! $settings{execute} ) {
		$$run = 0;
		return 0;
    }

    $$run = 1;
    $new->{reader_files} = 0;
    $new->{reader_events} = 0;
    $new->{missed_events} = 0;
    $new->{reader_sequence} = [ ];

    my $out = sprintf( "%s/reader-%02d.out", $dir, $loop );
    open( OUT, ">$out" );
	my $pipe;
	my $pid;
	if ( exists $opts->{"continue"} ) {
		$pid  = $state->{"pid"};
		$pipe = $state->{"pipe"};
		print "Continuing reader PID $pid\n";
		kill( $signos{CONT}, $state->{"pid"} ) or
			die "Can't send CONTINUE to ".$state->{"pid"};
		if ( $settings{valgrind_reader} ) {
			if ( open( VG, ">$vg_full.$$" ) ) {
				print VG "fake\n";
				close( VG );
			}
		}
	}
	else {
		$pid = open( $pipe, "$cmd 2>&1 |" );
		$pid or die "Can't run $cmd";
	}
	$state->{stopped} = 0;
    while( <$pipe> ) {
		print if ( $settings{verbose} > 1 );
		print OUT;
		chomp;
		if ( /^Read (\d+) events/ ) {
			$new->{reader_events} = $1;
		}
		elsif ( /Global JobLog:.*sequence=(\d+)/ ) {
			push( @{$new->{reader_sequence}}, $1 );
			$new->{reader_files}++;
		}
		elsif ( /SIGSTOP/ ) {
			$state->{stopped} = 1;
			last;
		}
		elsif ( /Missed event/ ) {
			$new->{missed_events}++;
			if ( exists $opts->{missed_ok} ) {
				if ( $settings{verbose} ) {
					print "Missed event(s) detected (OK)\n";
				}
			}
			else {
				printf "ERROR: unexpected missed event(s)\n";
				$errors++;
			}
		}
    }
    close( OUT );
	if ( $state->{stopped} ) {
		if ( ! exists $opts->{"stop"} ) {
			kill( $signos{KILL}, $pid );
			die "Reader processed $pid stopped unexpectedly; killed";
		}
		print "reader process stopped\n";
		$state->{"pid"}  = $pid;
		$state->{"pipe"} = $pipe;
	}
	else {
		close( $pipe );

		if ( $? & 127 ) {
			printf( "ERROR: reader (PID $pid) exited from signal %d\n",
					($? & 127) );
			$errors++;
		}
		if ( $? & 128 ) {
			print( "ERROR: reader (PID $pid) dumped core\n" );
			CopyCore( $reader_args[0], $pid, $dir );
			$errors++;
		}
		if ( $? >> 8 ) {
			printf( "ERROR: reader (PID $pid) exited with status %d\n",
					($? >> 8) );
			$errors++;
		}
		if ( ! $errors and $settings{verbose}) {
			print "reader process exited normally\n";
		}
	}

    if ( -f "$dir/reader.state" ) {
		my $state = sprintf( "%s/reader.state", $dir );
		my $copy = sprintf( "%s/reader-%02d.state", $dir, $loop );
		my @cmd = ( "/bin/cp", $state, $copy );
		system( @cmd );

		@cmd = ( $programs{reader_state}, "dump", $state );
		my $out = sprintf( "%s/reader-%02d.dump", $dir, $loop );

		if ( !open( OUT, ">$out" ) ) {
			print STDERR "Can't dump state to $out\n";
			return $errors;
		}
		my $cmd = join( " ", @cmd );
		if ( ! open( DUMP, "$cmd|" ) ) {
			print STDERR "Can't get dump state of $state\n";
			return $errors;
		}

		while( <DUMP> ) {
			print OUT;
		}
		close( DUMP );
		close( OUT );
    }

    if ( $settings{valgrind_reader} ) {
		$errors += CheckValgrind( $vg_out );
    }
    return $errors;
}

sub CopyCore( $$$ )
{
	my $prog = shift;
	my $pid = shift;
	my $dir = shift;

	my $core = "";
	if ( -f "core.$pid" ) {
		$core = "core.$pid";
	}
	elsif ( -f "core.$pid.0" ) {
		$core = "core.$pid";
	}
	elsif ( -f "core" ) {
		my $fileout = `/usr/bin/file core`;
		if ( defined $fileout and $fileout =~ /$prog/ ) {
			$core = "core";
		}
		else {
			my @statbuf = stat( "core" );
			if ( $#statbuf >= 9 and ( $statbuf[9] > (time()-10) ) ) {
				$core = "core";
			}
		}
	}

	if ( $core ne "" ) {
		print "Copying core to $dir\n";
		system( "cp $core $dir" );
	}
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

		elsif ( /FILE DESCRIPTORS: (\d+) open/ ) {
			my $open_fds = $1;
			my @lines = ( $_ );
			my @files;
			my $leaks = 0;
			while ( ($open_fds > 0) && (<VG>) ) {
				if ( /Open file descriptor (\d+):(.*)/ ) {
					push( @lines, $_ );
					$open_fds--;
					my $fd   = $1; chomp($fd);
					my $file = $2; chomp($file);
					$_ = <VG>;
					if ( ! $_ ) {
						print "End of file!\n";
						last;
					}
					push( @lines, $_ );
					if ( /inherited from parent/ ) {
						next;
					}
					else {
						$leaks++;
					}
					if ( len($file) ) {
						push( @files, "$fd=$file" );
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


# #######################################
# Check the writer's output
# #######################################
sub CheckWriterOutput( $$$$ )
{
    my $files = shift;
    my $loop  = shift;
    my $new   = shift;
    my $diff  = shift;

    my $errors = 0;

    if ( $new->{writer_events} < $expect{loop_mins}{num_events} ) {
		printf( STDERR
				"ERROR: loop %d: writer wrote too few events: %d < %d\n",
				$loop,
				$new->{writer_events}, $expect{loop_mins}{num_events} );
		$errors++;
    }
    if ( $new->{writer_sequence} < $expect{cur_mins}{sequence} ) {
		printf( STDERR
				"ERROR: loop %d: writer sequence too low: %d < %d\n",
				$loop,
				$new->{writer_sequence}, $expect{cur_mins}{sequence} );
		$errors++;
    }

    my $nfiles = scalar(@{$files});
    if ( $nfiles < $expect{cur_mins}{num_files} ) {
		printf( STDERR
				"ERROR: loop %d: too few actual files: %d < %d\n",
				$loop, $nfiles, $expect{cur_mins}{num_files} );
    }
    if ( ! $new->{events_lost} ) {
		if ( $diff->{num_events} < $expect{loop_mins}{num_events} ) {
			printf( STDERR
					"ERROR: loop %d: too few actual events: %d < %d\n",
					$loop,
					$diff->{num_events}, $expect{loop_mins}{num_events} );
			$errors++;
		}
    }
    if ( $new->{sequence} < $expect{cur_mins}{sequence} ) {
		printf( STDERR
				"ERROR: loop %d: actual sequence too low: %d < %d\n",
				$loop,
				$new->{sequence}, $expect{cur_mins}{sequence});
		$errors++;
    }

    if ( $new->{file_size} > $expect{maxs}{total_size} ) {
		printf( STDERR
				"ERROR: loop %d: total file size too high: %d > %d\n",
				$loop,
				$new->{total_size}, $expect{maxs}{total_size});
		$errors++;
    }

    return $errors;
}


# #######################################
# Check the writer's output
# #######################################
sub CheckReaderOutput( $$$$ )
{
    my $files = shift;
    my $loop  = shift;
    my $new   = shift;
    my $diff  = shift;

    my $errors = 0;

    if ( $new->{reader_events} < $new->{writer_events} ) {
		printf( STDERR
				"ERROR: loop %d: reader found fewer events than writer wrote".
				": %d < %d\n",
				$loop,
				$new->{reader_events}, $new->{writer_events} );
		$errors++;
    }

    my $nseq = $#{$new->{reader_sequence}};
    my $rseq = -1;
    if ( $nseq >= 0 ) {
		$rseq = @{$new->{reader_sequence}}[$nseq];
    }

	if ( $nseq < 0 ) {
		# do nothing
    }
    elsif ( $rseq < $new->{writer_sequence} ) {
		printf( STDERR
				"ERROR: loop %d: reader sequence too low: %d < %d\n",
				$loop,
				$rseq, $new->{writer_sequence} );
		$errors++;
    }
    elsif ( ($nseq+1) < $expect{loop_mins}{sequence} ) {
		printf( STDERR
				"ERROR: loop %d: reader too few sequences: %d < %d\n",
				$loop,
				$nseq+1, $expect{loop_mins}{sequence} );
		$errors++;
    }

    if ( $new->{reader_files} < $new->{writer_files} ) {
		printf( STDERR
				"ERROR: loop %d: reader found too few files: %d < %d\n",
				$loop,
				$new->{reader_files}, $new->{writer_files} );
    }

    return $errors;
}


# #######################################
# Read the eventlog files
# #######################################
sub ReadEventlogs( $$$ )
{
    my $dir = shift;
    my $new = shift;
	my $files = shift;

	my $errors = 0;
    my @header_fields = qw( ctime id sequence size events offset event_off );

    opendir( DIR, $dir ) or die "Can't read directory $dir";
    while( my $t = readdir( DIR ) ) {
		if ( $t =~ /^EventLog(\.old|\.\d+)*$/ ) {
			$new->{num_files}++;
			my $ext = $1;
			my $rot = 0;
			if ( defined $ext  and  $ext eq ".old" ) {
				$rot = 1;
			}
			elsif ( defined $ext  and  $ext =~ /\.(\d+)/ ) {
				$rot = $1;
			}
			else {
				$rot = 0;
			}
			my $f = { name => $t, ext => $ext, num_events => 0 };
			@{$files}[$rot] = $f;
			my $file = "$dir/$t";
			open( FILE, $file ) or die "Can't read $file";
			while( <FILE> ) {
				chomp;
				if ( $f->{num_events} == 0  and  /^008 / ) {
					$f->{header} = $_;
					$f->{fields} = { };
					foreach my $field ( split() ) {
						if ( $field =~ /(\w+)=(.*)/ ) {
							$f->{fields}{$1} = $2;
						}
					}
					foreach my $fn ( @header_fields ) {
						if ( not exists $f->{fields}{$fn} ) {
							print STDERR
								"ERROR: header in file $t missing $fn\n";
							$errors++;
							$f->{fields}{$fn} = 0;
						}
					}
				}
				if ( $_ eq "..." ) {
					$f->{num_events}++;
					$new->{num_events}++;
				}
			}
			close( FILE );
			$f->{file_size} = -s $file;
			$new->{total_size} += -s $file;
		}
    }
    closedir( DIR );
    return $errors;
}

# #######################################
# Read the eventlog files, process them
# #######################################
sub ProcessEventlogs( $$ )
{
    my $files = shift;
    my $new = shift;

	my $errors = 0;

    my $events_counted =
		( exists $test->{config}{EVENT_LOG_COUNT_EVENTS} and
		  $test->{config}{EVENT_LOG_COUNT_EVENTS} eq "TRUE" );

    my $min_sequence = 9999999;
    my $min_event_off = 99999999;
    my $maxfile = $#{$files};
    foreach my $n ( 0 .. $maxfile ) {
		if ( ! defined ${$files}[$n] ) {
			print STDERR "ERROR: EventLog file #$n is missing\n";
				$errors++;
			next;
		}
		my $f = ${$files}[$n];
		my $t = $f->{name};
		if ( not exists $f->{header} ) {
			print STDERR "ERROR: no header in file $t\n";
			$errors++;
		}
		my $seq = $f->{fields}{sequence};

		# The sequence on all but the first (chronologically) file
		# should never be one
		if (  ( $n != $maxfile ) and ( $seq == 1 )  ) {
			print STDERR
				"ERROR: sequence # for file $t (#$n) is $seq\n";
				$errors++;
		}
		# but the sequence # should *never* be zero
		elsif ( $seq == 0 ) {
			print STDERR
				"ERROR: sequence # for file $t (#$n) is zero\n";
				$errors++;
		}
		if ( $seq > 1 ) {
			if ( $f->{fields}{offset} == 0 ) {
				print STDERR
					"ERROR: offset for file $t (#$n / seq $seq) is zero\n";
				$errors++;
			}
			if ( $events_counted  and  $f->{fields}{event_off} == 0 ) {
				print STDERR
					"ERROR: event offset for file $t ".
					"(#$n / seq $seq) is zero\n";
				$errors++;
			}
		}
		if ( $seq > $new->{sequence} ) {
			$new->{sequence} = $seq;
		}
		if ( $seq < $min_sequence ) {
			$min_sequence = $seq;
		}
		if ( $f->{fields}{event_off} < $min_event_off ) {
			$min_event_off = $f->{fields}{event_off};
		}
		if ( $events_counted ) {
			if ( $n  and  $f->{fields}->{events} == 0 ) {
				print STDERR
					"ERROR: # events for file $t (#$n / seq $seq) is zero\n";
					$errors++;
			}
		}
		else {
			if ( $f->{fields}->{events} != 0 ) {
				printf( STDERR
						"ERROR: event count for file $t ".
						"(#$n / seq $seq) is non-zero (%d)\n",
						$f->{fields}->{events} );
				$errors++;
			}
			if ( $f->{fields}->{event_off} != 0 ) {
				printf( STDERR
						"ERROR: event offset for file $t ".
						"(#$n / seq $seq) is non-zero (%d)\n",
						$f->{fields}->{event_off} );
				$errors++;
			}
		}

		if ( $n  and  ( $f->{file_size} > $expect{maxs}{file_size} ) ) {
			printf STDERR
				"ERROR: File $t is over size limit: %d > %d\n",
				$f->{file_size},  $expect{maxs}{file_size};
			$errors++;
		}

    }

    if ( $min_sequence > 0 ) {
		$new->{events_lost} = 1;
		if ( $min_event_off > 0 ) {
			$new->{num_events_lost} = $min_event_off;
		}
		else {
			$new->{num_events_lost} = -1;
		}
    }
	return $errors;
}

# #######################################
# Final checks
# #######################################
sub FinalCheck(  )
{
	my $errors = 0;

	printf "\n** Starting final analysis @ %s **\n", $timestr;

	# Final writer output checks
	if ( $totals{writer_events} < $expect{final_mins}{num_events} ) {
		printf( STDERR
				"ERROR: final: writer wrote too few events: %d < %d\n",
				$totals{writer_events}, $expect{final_mins}{num_events} );
		$errors++;
	}
	if ( $settings{run_reader} and
		 ( !$totals{missed_events} ) and
		 ( $totals{reader_events} < $totals{writer_events} )  ) {
		printf( STDERR
				"ERROR: final: read fewer events than writer wrote: %d < %d\n",
				$totals{reader_events}, $totals{writer_events} );
		$errors++;
	}
	if ( $settings{run_reader} ) {
		printf( STDERR "final: reader read %d events, writer wrote %d\n",
				$totals{reader_events}, $totals{writer_events} );
	}

	$totals{seq_files} = $totals{sequence} + 1;
	if ( $totals{writer_sequence} < $expect{final_mins}{sequence} ) {
		printf( STDERR
				"ERROR: final: writer sequence too low: %d < %d\n",
				$totals{writer_sequence}, $expect{final_mins}{sequence} );
		$errors++;
	}

	# Final counted checks
	my %final;
	foreach my $k ( keys(%totals) ) {
		$final{$k} = 0;
	}
	my @files;
	my $tmp_errors;

	$tmp_errors = ReadEventlogs( $dir, \%final, \@files );
	if ( $errors ) {
		print STDERR "ERROR: final: ReadEventLogs()\n";
		$total_errors += $errors;
	}
	$tmp_errors = ProcessEventlogs( \@files, \%final );
	if ( $tmp_errors ) {
		print STDERR "ERROR: final: ProcessEventLogs()\n";
		$errors += $tmp_errors;
	}
	if ( scalar(@files) < $expect{final_mins}{num_files} ) {
		printf( STDERR
				"ERROR: final: too few actual files: %d < %d\n",
				scalar(@files), $expect{final_mins}{num_files} );
	}
	if ( ! $final{events_lost} ) {
		if ( $final{num_events} < $expect{final_mins}{num_events} ) {
			printf( STDERR
					"ERROR: final: too few actual events: %d < %d\n",
					$final{num_events}, $expect{loop_mins}{num_events} );
			$errors++;
		}
	}
	if ( $final{sequence} < $expect{final_mins}{sequence} ) {
		printf( STDERR
				"ERROR: final: actual sequence too low: %d < %d\n",
				$final{sequence}, $expect{final_mins}{sequence});
		$errors++;
	}

	if ( $final{file_size} > $expect{maxs}{total_size} ) {
		printf( STDERR
				"ERROR: final: total file size too high: %d > %d\n",
				$final{total_size}, $expect{maxs}{total_size});
		$errors++;
	}
}


### Local Variables: ***
### mode:perl ***
### tab-width: 4  ***
### End: ***
