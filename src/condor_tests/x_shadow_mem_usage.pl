#! /usr/bin/env perl

# dump some stuff about the environment we are running in.
if (0) {
  print "pwd\n";
  print("pwd");
  print "cat $ENV{_CONDOR_MACHINE_AD}\n";
  system("cat $ENV{_CONDOR_MACHINE_AD}");
  print "cat $ENV{_CONDOR_JOB_AD}\n";
  system("cat $ENV{_CONDOR_JOB_AD}");
}

my $condor_chirp=`awk '/^ChirpPath/{print \$3}' $ENV{_CONDOR_MACHINE_AD}`;
chomp($condor_chirp); $condor_chirp =~ s/"//g;
print "condor_chirp is \"$condor_chirp\"\n";

my $submit_platform=`awk '/^CondorPlatform/{print \$4}' $ENV{_CONDOR_JOB_AD}`;
chomp($submit_platform);
print "submit_platform is \"$submit_platform\"\n";

if (($^O =~ /MSWin32/) || ($^O =~ /cygwin/)) {
   # we don't expect to run this test on windows.
} else {
  $shadow_pid = `$condor_chirp getdir /proc/self/task`; chomp($shadow_pid);
  print "shadow_pid is $shadow_pid.\n";
  system("$condor_chirp ulog 'shadow_pid = $shadow_pid'");

  print "\n**** /proc/self/status ****\n";
  system("$condor_chirp read /proc/self/status 4096");
  #print "\n**** /proc/self/smaps ****\n";
  #system("$condor_chirp read /proc/self/smaps 1000000");
  print "\n**** end ****\n";
  
}

# sleep to give time for the shadow to react to ulog message
sleep(20);
exit 0;
