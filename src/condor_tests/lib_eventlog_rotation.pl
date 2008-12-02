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
	"  -n|--no-exec  no execute / test mode\n" .
	"  -d|--debug    enable D_FULLDEBUG debugging\n" .
	"  -v|--verbose  increase verbose level\n" .
	"  --vg-writer   run writer under valgrind\n" .
	"  -q|--quiet    cancel debug & verbose\n" .
	"  -h|--help     this help\n";
}

sub ReadEventlogs( $$ );
sub ProcessEventlogs( $$ );

my %settings =
(
 verbose		=> 0,
 debug			=> 0,
 execute		=> 1,
 force			=> 0,
 valgrind_writer	=> 0,
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
    elsif ( $arg eq "-n"  or  $arg eq "--vg-writer" ) {
	$settings{valgrind_writer} = 1;
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


# Expected results
my %expect = (
	      final_mins => {
		  num_files	=> $total_files,
		  sequence	=> $total_files,
		  num_events	=> $loop_events * $total_loops,
		  file_size	=> $max_size,
	      },
	      maxs => {
		  file_size	=> $max_size + 256,
		  total_size	=> ($max_size + 256) * $total_files,
	      },
	      loop_mins => {
		  num_files	=> int($total_files / $total_loops),
		  sequence	=> int($total_files / $total_loops),
		  file_size	=> $max_size / $total_loops,
		  num_events	=> $loop_events,
	      },
	      cur_mins => {
		  num_files	=> 0,
		  sequence	=> 0,
		  file_size	=> 0,
		  num_events	=> 0,
	      },
	      );


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
foreach my $n ( 1 .. $settings{verbose} ) {
    push( @writer_args, "-v" );
}
if ( $settings{debug} ) {
    push( @writer_args, "--debug" );
    push( @writer_args, "D_FULLDEBUG" );
}
if ( exists $test->{writer}{file} ) {
    push( @writer_args, $test->{writer}{file} )
}
else {
    push( @writer_args, "/dev/null" )
}

my @valgrind = ( "valgrind", "--tool=memcheck", "--num-callers=24",
		 "-v", "-v", "--leak-check=full",
		 "--log-file=$dir/valgrind-writer.out" );

system("date");
my $errors = 0;

# Total writer out
my %totals = ( sequence => 0,
	       num_events => 0,
	       max_rot => 0,
	       file_size => 0,

	       events_lost => 0,
	       num_events_lost => 0,

	       writer_events => 0,
	       writer_sequence => 0,
	       );

# Actual from previous loop
my %previous;
foreach my $k ( keys(%totals) ) {
    $previous{$k} = 0;
}

foreach my $loop ( 1 .. $test->{loops} )
{
    foreach my $k ( keys(%{$expect{loop_mins}}) ) {
	$expect{cur_mins}{$k} += $expect{loop_mins}{$k};
    }

    # writer output from this loop
    my %new;
    for my $k ( keys(%totals) ) {
	$new{$k} = 0;
    }

    # Run the writer
    my ($run, $failed) = RunWriter( \%new, \%previous, \%totals, \$errors );
    if ( ! $run ) {
	next;
    }
    if ( $failed ) {
	print STDERR "writer failed: aborting test\n";
	last;
    }

    my @files = ReadEventlogs( $dir, \%new );
    ProcessEventlogs( \@files, \%new );

    my %diff;
    foreach my $k ( keys(%previous) ) {
	$diff{$k} = $new{$k} - $previous{$k};
	$totals{$k} += $new{$k};
    }
    $errors += CheckWriterOutput( \@files, $loop, \%new, \%diff );

    foreach my $k ( keys(%previous) ) {
	$previous{$k} = $new{$k};
    }
}

if ( ! $settings{execute} ) {
    exit( 0 );
}

# Final writer output checks
if ( $totals{writer_events} < $expect{final_mins}{num_events} ) {
    printf( STDERR
	    "ERROR: final: writer wrote too few events: %d < %d\n",
	    $totals{writer_events}, $expect{final_mins}{num_events} );
    $errors++;
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
my @files = ReadEventlogs( $dir, \%final );
ProcessEventlogs( \@files, \%final );
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


if ( $errors  or  $settings{verbose}   or  $settings{debug} ) {
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


# #######################################
# Run the writer
# #######################################
sub RunWriter( $$$$ )
{
    my $new = shift;
    my $previous = shift;
    my $totals = shift;
    my $errors = shift;

    my $cmd = "";
    if ( $settings{valgrind_writer} ) {
	$cmd .= join( " ", @valgrind ) . " ";
    }
    $cmd .= join(" ", @writer_args );
    print "$cmd\n";

    if ( ! $settings{execute} ) {
	return ( 0, 0 );
    }

    open( WRITER, "$cmd|" ) or die "Can't run $cmd";
    while( <WRITER> ) {
	print if ( $settings{verbose} );
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

    my $failed = 0;
    if ( $? & 127 ) {
	printf "ERROR: writer exited from signal %d\n", ($? & 127);
	$$errors++;
	$failed = 1;
    }
    if ( $? & 128 ) {
	print "ERROR: writer dumped core\n";
	$$errors++;
	$failed = 1;
    }
    if ( $? >> 8 ) {
	printf "ERROR: writer exited with status %d\n", ($? >> 8);
	$$errors++;
	$failed = 1;
    }
    if ( ! $failed and $settings{verbose}) {
	print "writer process exited normally\n";
    }

    return ( 1, $failed );
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
# Read the eventlog files
# #######################################
sub ReadEventlogs( $$ )
{
    my $dir = shift;
    my $new = shift;

    my @header_fields = qw( ctime id sequence size events offset event_off );

    my @files;
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
	    $files[$rot] = { name => $t, ext => $ext, num_events => 0 };
	    my $f = $files[$rot];
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
    return @files;
}

# #######################################
# Read the eventlog files, process them
# #######################################
sub ProcessEventlogs( $$ )
{
    my $files = shift;
    my $new = shift;

    my $events_counted = 
	( exists $test->{config}{EVENT_LOG_COUNT_EVENTS} and
	  $test->{config}{EVENT_LOG_COUNT_EVENTS} eq "TRUE" );

    my $min_sequence = 9999999;
    my $min_event_off = 99999999;
    my $maxfile = $#{@{$files}};
    foreach my $n ( 0 .. $maxfile ) {
	if ( ! defined ${@{$files}}[$n] ) {
	    print STDERR "ERROR: EventLog file #$n is missing\n";
	    $errors++;
	    next;
	}
	my $f = ${@{$files}}[$n];
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
}
