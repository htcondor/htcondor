#!/usr/bin/perl
use strict; use warnings;
use Digest::MD5 qw(md5_base64);
use vars qw($condor_chirp_binary);
# What doesn't work:
#  0. Permissions don't seem to get set correctly.
#  1. -wa doesn't work (append mode)
my $condor_chirp_binary = 'condor_chirp';

sub condor_chirp { return system("$condor_chirp_binary ".$_[0]); }

#####################################################
# Fetch 
my $fetch_fn =   "cmd_chirp_fetchput_van.temp.fetchme";
# Common fetch usage
condor_chirp("fetch  $fetch_fn $fetch_fn") and # ('and' since 0 is success)
	die "Fetch: condor_chirp did not execute successfully.";
sub verify_fetched_file {
	my ($filename,$ft) = @_; #$ft is used to distinguish error messages between the 2 tests
	open FILE, $filename or die "Fetch: fetch $ft does not exist or is inaccessible.";
	my @fetch_contents = <FILE>;
	close FILE or die "Fetch: could not close $ft."; 
	### Now we verify the fetch contents... ###
	scalar(@fetch_contents)==2 or die "Fetch: fetched $ft wrong length."; # check length 
	chomp $fetch_contents[0]; # first line should be md5 of the second line --v
	$fetch_contents[0] eq md5_base64($fetch_contents[1]) or die "Fetch: md5 fail on $ft.";
}
# The first call of this sub verifies the fetch above this sub
verify_fetched_file($fetch_fn, 'file');
# The second fetch both calls fetch and verifies it, checking STDIN fetch usage 
verify_fetched_file("$condor_chirp_binary fetch $fetch_fn - |", 'STDIN');
#####################################################

#####################################################
# Remove
my $remove_fn =  "cmd_chirp_fetchput_van.temp.removeme";
condor_chirp("remove $remove_fn") and
	die "Remove: condor_chirp did not execute successfully.";
#####################################################

#####################################################
# Put (various permissions and mode options)
my $put_fn_0 = "cmd_chirp_fetchput_van.temp.putme.0";
my $put_fn_1 = "cmd_chirp_fetchput_van.temp.putme.1";
my $put_fn_2 = "cmd_chirp_fetchput_van.temp.putme.2";
my $example_contents = "example line\n";
open QUICK_FILE, ">>$put_fn_1" or die "Could not open local file.";
print QUICK_FILE "$example_contents";
close QUICK_FILE;
# First we test a basic put
condor_chirp("put $fetch_fn $put_fn_0") and
	die "put: condor_chirp did not execute successfully. (0)"; 
# Now we test to see if the x mode prevents overwrite properly 	
condor_chirp("put -mode wcx $fetch_fn $put_fn_0") or 
	die "put: condor_chirp should have failed as it had -mode cx and ". 
		"should have clobbered a file, however it didn't fail. (0)";
# Now we'll test append mode, and a different permission set, on a different file
# $put_fn_1 on the submitter side now should have "example line\n"x2
condor_chirp("put -mode wcx -perm 0755 $put_fn_1 $put_fn_1") and 
	die "put: condor_chirp did not execute successfully. (1) (Did last test not " +
	"leave a file named $put_fn_1?)"; 
condor_chirp("put -mode wa $put_fn_1 $put_fn_1") and 
	die "put: condor_chirp did not execute successfully. (1 append)"; 
# Finally, we'll test another permissions, and this time use 
# standard in as the input.
open (CONDOR_CHIRP, "| $condor_chirp_binary put -perm 0655 -mode wcx - $put_fn_2") or 
	die "put: condor_chirp did not execute successfully. (2)";
print CONDOR_CHIRP "$example_contents";
close CONDOR_CHIRP or die "put: condor_chirp stream did not close. (2)";
#####################################################
exit(0);
