#!/usr/bin/perl

####################################################################################################
# app_post_success.pl
# post script for a successful Condor testsuite run   
# 06/12/04 bgietzel@cs.wisc.edu 
####################################################################################################

####################################################################################################
# set up path 
####################################################################################################

my $HomeDir = $ENV{HOME};
my $MakeDir = $ENV{SRC_DIR};
print "PATH is $ENV{PATH}\n";

####################################################################################################
# tar up test results
####################################################################################################

#note - tar with 2 options fails on a lot of the platforms (-cr)  
# we should be using gnu tar for all - make sure path is correct
$logs = "logs.tar.gz";
$results = "results.tar.gz";
chdir("$HomeDir");

print "Tarring up debug stuff - condor logs, config files and daemons\n";
&verbose_system("tar zcf $HomeDir/$logs condor local");

print "Tarring up debug stuff - condor logs, config files and daemons\n";
&verbose_system("tar zcf $HomeDir/$results condor/etc/condor_config results");


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

