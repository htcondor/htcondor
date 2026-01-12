#!/usr/bin/env perl

# This script will run the tests against a personal HTCondor.
# before running this script you must
#  * build condor and make install to get a release_dir OR unpack the .zip or .tar file from a build
#  * configure the personal HTCondor (i.e run condor_install, or configure "use ROLE: Personal", and make sure that HTCondor starts)
#  * unpack the condor_tests tarball into $(LOCAL_DIR)\condor_tests OR run the prep_for_tests.pl script
#  * set the CONDOR_CONFIG and PATH environment variables to point to the personal HTCondor
#  * (optional) start the personal HTCondor (many of the tests will run faster if it is already started)
#  * run this script.
# When this script runs, it will build tests.lst if it does not already exist.
# it will then load tests.lst and done_tests.lst
# tests that are in the first list but not in the second will be run
# Test completion status will be appended to done_tests.lst
# 

use strict;
use warnings;
use File::Basename;
use File::Spec;

my $dry_run = 0;
my $run_tests = 0;
my $do_init = 0;
my $created_test_list = 0;
my $created_ctest_list = 0;
my $testdir = "condor_tests";

if (@ARGV) {
    while (<@ARGV>) {
        if ($_ eq '-init') {
            $do_init = 1;
            $run_tests = 0;
        } elsif ($_ eq '-all') {
            $run_tests = 1;
        } elsif ($_ eq '-dry') {
            $run_tests = 1;
            $dry_run = 1;
        } else {
            print("unknown arg $_\n");
            exit(0);
        }
    }
} else {
    print ("Usage: $0 [-init] [-all] [-dry]\n");
    print ("  -init  build tests.lst and $testdir/CTestTestfile.cmake\n");
    print ("  -all   run all tests in tests.lst\n");
    print ("  -dry   dry-run all tests in tests.lst\n");
    print ("To use ctest, run with -init, then cd to $testdir and run ctest\n");
    print ("Note that -init will create but not overwrite tests.lst or $testdir/CTestTestfile.cmake\n");
    exit(0);
}

my $resume; # = "cmd_q_formats";

# populate the tests array, either by reading it from tests.lst
# or by creating tests.lst from $testdir\list_quick and then readit it.
#
my @tests;
my $taskfile = "tests.lst";
if ( ! -f $taskfile) { create_tasklist($taskfile); }
open(TASKS, '<', $taskfile) || die "Can't open test list $taskfile for read: $!\n";
fullchomp (@tests = <TASKS>);
close TASKS;

# now that we have the @tests array populated, we can create a ctest list
my $ctestfile = "$testdir/CTestTestfile.cmake";
if ( ! -f $ctestfile) { create_ctestfile($ctestfile); }

if ($do_init) {
   my $num_tests = scalar(@tests);
   print "created $taskfile with $num_tests tests\n" if ($created_test_list);
   print "created $ctestfile with $num_tests tests\n" if ($created_ctest_list);
}
if ( ! $run_tests) { exit(0); }

my $donefile = "done_tests.lst";
my %skips = ();
if (-f $donefile) { %skips = load_map($donefile); }
open (DONE, '>>', $donefile);
binmode DONE; # prevent \r from being injected

# now run the tests in the @tests array

chdir($testdir) || die "failed to chdir to $testdir\n";

foreach my $test (@tests) {

   if ($test =~ /^\s*#/) {
      print "skipping $test\n";
      next;
   }
   if (defined($resume) && ($resume ne $test)) {
      print "skipping past $test\n";
      next;
   }
   undef $resume;

   if (defined($skips{$test})) {
      my $stat = $skips{$test};
      if ($stat eq "1") { $stat = "done"; }
      print "skipping $stat $test\n";
      next;
   }

   print "\n\n============ $test ===============\n";
   my $start_time = time();
   my $wait_code = runit("perl run_test.pl $test\n");
   my $duration = time() - $start_time;
   my $exit_code = $wait_code >> 8;
   my $sig_code = $wait_code & 255;
   my $signal = ""; if ($sig_code != 0) { $signal = "sig$sig_code"; }
   print "\nexit = $exit_code $signal\n============ end $test ===============\n";
   if ($wait_code != 0) {
      if (-f "$test.run.out") {
         system("tail -20 $test.run.out\n");
         print "\n============ end $test.run.out ===============\n";
      }
      print DONE "$test FAILED $exit_code $signal ($duration sec)\n";
   #   last;
   } else {
      print DONE "$test succeeded ($duration sec)\n";
   }
}

close DONE;

my %tasklist;
sub create_tasklist {

   my $file = File::Spec->catfile(@_);

   # load quick tests list
   %tasklist = load_list($testdir, "list_quick");

   # load skip list
   my %skiplist = ();
   if (is_windows()) { %skiplist = load_list($testdir, "Windows_SkipList"); }

   # set rough test priorities
   foreach (keys %tasklist) {
      if ($_ =~ /unit_test/) { $tasklist{$_} += 20; }
      if ($_ =~ /basic/) { $tasklist{$_} += 7; }
      if ($_ =~ /^config/) { $tasklist{$_} += 4; }
      if ($_ =~ /^lib_/) { $tasklist{$_} += 3; }
      if ($_ =~ /^cmd_/) { $tasklist{$_} += 2; }
      if ($_ =~ /_core_/) { $tasklist{$_} += 2; }
      if ($_ =~ /^job_f/) { $tasklist{$_} += 1; }
      if ($_ =~ /concurrency/) { $tasklist{$_} -= 1; } # these are very slow tests
   }

   # iterate the tests into the tasklist in prio order
   #
   open (TASKS, '>', $file) || die "Cannot open test list $file for write: $!\n";
   binmode TASKS;

   foreach my $task (sort prio_sort keys %tasklist) {
      my $skip = defined($skiplist{$task}) ? 1 : 0;
      if ($skip) {
         next;
      }

      print TASKS "$task\n";
   }
   close TASKS;
   $created_test_list = 1;
}

sub create_ctestfile {

   my $file = File::Spec->catfile(@_);
   my $this_perl = $^X;
   $this_perl =~ s/\\/\//g; # convert \ to /

   open (CTESTFILE, '>', $file) || die "Cannot open Ctest list $file for write: $!\n";
   binmode CTESTFILE;

   #add_test([=[job_hyperthread_check]=] "/path/to/perl" "run_test.pl" "job_hyperthread_check")
   foreach my $test (@tests) {
      my $add = 'add_test([=[' . $test . ']=] ';
         $add .= '"' . $this_perl . '" ';
         $add .= '"run_test.pl" ';
         $add .= '"' . $test . '"';
         $add .= ')';
      print CTESTFILE "$add\n";
   }

   close CTESTFILE;
   $created_ctest_list = 1;
}

sub load_list {
   my $list = File::Spec->catfile(@_);
   open (LIST, '<', $list) || die "cannot open $list for read: $!\n";
   return map { $_ =~ s/\s+$//; "$_" => 1 } <LIST>;
}

sub load_map {
   my $list = File::Spec->catfile(@_);
   open (LIST, '<', $list) || die "cannot open $list for read: $!\n";
   my %hash;
   for (<LIST>) {
     next if ($_ =~ /^\s*$/); # ignore blank lines
     $_ =~ s/\s+$//; # remove all trailing whitespace
     $hash{$_} = 1;
     if ($_ =~ /^\s*([\-\w]+)\s*([\w]+)\s*(.*)$/) {
        $hash{$1} = $2;
     }
   }
   return %hash;
}

sub prio_sort {
  my $ret = $tasklist{$b} <=> $tasklist{$a};
  if ( ! $ret) { $ret = $a cmp $b; }
  return $ret;
}

sub is_windows {
   if (($^O =~ /MSWin32/)) { return 1; }
   return 0;
}

sub fullchomp {
   push (@_,$_) if (scalar(@_) == 0);
   foreach my $arg (@_) { $arg =~ s/[\012\015]+$//; }
   return;
}

sub runit {
   my $ret = 0;
   if ($dry_run) { print(@_); $ret = 0; }
   else { $ret = system(@_); }
   return $ret;
}
