#!/usr/bin/perl

######################################################################
# $Id: remote_task.pl,v 1.1.2.2 2004-06-24 00:58:02 wright Exp $
# run a test in the Condor testsuite
# return val is the status of the test
# 0 = good
# 1 = build failed
# 2 = run failed
######################################################################

my $fulltestname = $ARGV[0];
my $teststatus = 2; # set to failed by default

######################################################################
# set up path 
######################################################################

my $HomeDir = $ENV{HOME};
my $MakeDir = $ENV{SRC_DIR};
print "PATH is $ENV{PATH}\n";

######################################################################
# get the testname and group
######################################################################

@testinfo = split(/\//, $fulltestname);
my $compiler = @testinfo[0];
my $testname = @testinfo[1];
print "compiler is $compiler\n";
print "testname is $testname\n";

if ( ($compiler =~ m/gcc/) || ($compiler =~ m/g\+\+/) || ($compiler =~ /cc/) || ($compiler =~ /CC/) ){
  $testdir = "condor_test_suite_C.V5";
} elsif ( ($compiler =~ /g77/) || ($compiler =~ /f77/) || ($compiler =~ /f90/) ){
  $testdir = "condor_test_suite_F.V5";
} else {
  die "compiler $compiler is not valid\n";
}

######################################################################
# build the test
######################################################################

chdir("$MakeDir/$testdir/$compiler");
open ( TESTBUILD, "make $testname 2>&1 |");
while ( <TESTBUILD> ) {
  print $_;
}
close (TESTBUILD);
$buildstatus = $?;
print "BUILD TEST returned $buildstatus\n";
if ($buildstatus != 0) {
  $teststatus = 1;
} else {
  $teststatus = 0;
} 

######################################################################
# run the test using batch_test.pl
######################################################################

if ($buildstatus == 0){
  print "RUNNING $compiler/$testname\n";
  chdir("$MakeDir/$testdir");

  if ($testdir eq "condor_test_suite_C.V5") {
      open(BATCHTEST, "perl ./batch_test.pl -d $compiler -t $testname 2>&1 |" );
  } elsif ($testdir eq "condor_test_suite_F.V5") {
      open( BATCHTEST, "perl ../condor_test_suite_C.V5/batch_test.pl -d $compiler -t $testname 2>&1 |" );
  }

  while ( <BATCHTEST> ) {
    print $_;
  }
  close (BATCHTEST);
  $batchteststatus = $?;
}

# figure out here if the test passed or failed.  
if ($batchteststatus == 0) {
  $teststatus = 2;
}

######################################################################
# copy test results to results dir
######################################################################

system ("mkdir -p $HomeDir/results");
chdir ("$MakeDir/$testdir");
system ("cp $compiler/$testname.out $HomeDir/results");
system ("cp $compiler/$testname.error $HomeDir/results");
system ("cp $compiler/$testname.log $HomeDir/results");


sub verbose_system {

  my @args = @_;
  my $rc = 0xffff & system @args;

  printf "system(%s) returned %#04x: ", @args, $rc;

  if ($rc == 0) {
   print "ran with normal exit\n";
   return $rc;
  }
  elsif ($rc == 0xff00) {
   print "command failed: $!\n";
   return $rc;
  }
  elsif (($rc & 0xff) == 0) {
   $rc >>= 8;
   print "ran with non-zero exit status $rc\n";
   return $rc;
  }
  else {
   print "ran with ";
   if ($rc &   0x80) {
       $rc &= ~0x80;
       print "coredump from ";
   return $rc;
   }
   print "signal $rc\n"
  }
}

