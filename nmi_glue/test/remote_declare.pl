#!/usr/bin/perl

######################################################################
# $Id: remote_declare.pl,v 1.1.2.2 2004-06-24 00:58:02 wright Exp $
# generate list of all tests to run
######################################################################

my $testlocation;
my $list;

######################################################################
# set up path 
######################################################################

my $HomeDir = $ENV{HOME};
my $MakeDir = $ENV{SRC_DIR};
print "PATH is $ENV{PATH}\n";

######################################################################
# generate makefile, compiler_list and run_list for each test group 
######################################################################

print "GENERATE makefile, compiler_list and run_list for each test group \n"; 

chdir( $MakeDir );
my $testdirs = ();
push (@testdirs, "condor_test_suite_C.V5");
push (@testdirs, "condor_test_suite_F.V5");

open( TASKLIST, "tasklist.nmi" ) || die "cannot open tasklist.nmi\n"; 
foreach $testdir (@testdirs) {
  chdir( "$MakeDir/$testdir" );
  system( "make Makefile" );
  system( "make compiler_list" );
  open( "COMPILERS", "compiler_list" ) || die "cannot open compiler_list\n";
  while (<COMPILERS>){
    chomp($_);
    $compiler = $_;
    system( "make $compiler/Makefile" );
    system( "cd $compiler" );
    system( "make run_list" );
    open( "RUNLIST", "run_list" ) || die "cannot open run_list\n";
    while (<RUNLIST>) {
      chomp($_);
      $testname = $_;
      print TASKLIST "$compiler/$testname";
    }
    close (RUNLIST);
  } 
  close (COMPILERS);
}     
  
close (TASKLIST);


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

