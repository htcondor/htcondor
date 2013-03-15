#! /usr/bin/env perl

my $libexecdir = `condor_config_val LIBEXEC`;
chomp($libexecdir);
my $condor_chirp = "$libexecdir/condor_chirp";

my $name = "shadow";
if (@ARGV > 1) { $name = $ARGV[0]; }

if (($^O =~ /MSWin32/) || ($^O =~ /cygwin/)) {
   # we don't expect to run this test on windows.
} else {
  system("$condor_chirp fetch /proc/self/status $name.status");
  system("$condor_chirp fetch /proc/self/smaps $name.smaps");
}
exit 0;
