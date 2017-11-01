#!/usr/bin/env perl

# This script will run the tests against a personal HTCondor.
# before running this script you must
#  * build condor and make install to get a release_dir OR unpack the .zip or .tar file from a build
#  * configure the personal HTCondor (i.e run condor_install, or configure "use ROLE: Personal", and make sure that HTCondor starts)
#  * unpack the condor_tests tarball into $(LOCAL_DIR)\condor_tests OR run the prep_for_tests.pl script
#  * set the CONDOR_CONFIG and PATH environment variables to point to the personal HTCondor
#  * (optional) start the personal HTCondor (many of the tests will run faster if it is already started)
#  * run this script.
# When this script runs, it will build do_tests.lst if it does not already exist.
# it will then load do_tests.lst and done_tests.lst
# tests that are in the first list but not in the second will be run
# Test completion status will be appended to done_tests.lst
# 

use strict;
use warnings;
use File::Basename;
use File::Spec;

my $testdir = "condor_tests";

my $resume; # = "cmd_q_formats";

# populate the tests array, either by reading it from do_tests.lst
# or by creating do_tests.lst from condor_tests\list_quick and then readit it.
#
my @tests;
my $taskfile = "do_tests.lst";
if ( ! -f $taskfile) { create_tasklist($taskfile); }
open(TASKS, '<', $taskfile) || die "Can't open test list $taskfile for read: $!\n";
chomp (@tests = <TASKS>);
close TASKS;

my $donefile = "done_tests.lst";
my %skips = ();
if (-f $donefile) { %skips = load_map($donefile); }
open (DONE, '>>', $donefile);

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
   my $wait_code = system("perl run_test.pl $test\n");
   my $sig_code = $wait_code & 255;
   my $exit_code = $wait_code >> 8;
   print "\nexit = $exit_code (sig:$sig_code)\n============ end $test ===============\n";
   if ($wait_code != 0) {
      system("tail -20 $test.run.out\n");
      print "\n============ end $test.run.out ===============\n";
      print DONE "$test FAILED\n";
   #   last;
   } else {
      print DONE "$test succeeded\n";
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

   foreach my $task (sort prio_sort keys %tasklist) {
      my $skip = defined($skiplist{$task}) ? 1 : 0;
      if ($skip) {
         next;
      }

      print TASKS "$task\n";
   }
   close TASKS;
}


sub load_list {
   my $list = File::Spec->catfile(@_);
   open (LIST, '<', $list) || die "cannot open $list for read: $!\n";
   return map { chomp; "$_" => 1 } <LIST>;
}

sub load_map {
   my $list = File::Spec->catfile(@_);
   open (LIST, '<', $list) || die "cannot open $list for read: $!\n";
   my %hash;
   for (<LIST>) {
     chomp;
     $hash{$_} = 1;
     if ($_ =~ /^\s*([\-\w]+)\s*(.*)$/) {
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
