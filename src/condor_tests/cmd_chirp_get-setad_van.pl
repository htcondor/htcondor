#!/usr/bin/perl
use strict;
use warnings;
use vars qw($condor_chirp_binary);
use Digest::MD5 qw(md5_base64);
my $condor_chirp_binary = './condor_chirp';
sub condor_chirp { return system("$condor_chirp_binary ".$_[0]); }
#####################################################
# get_job_attr
# Here we open a stream to the condor chirp binary executed with correct
# This is because get_job_attr is the only command in which we need to check 
# output on standard out (the rest we can just use system() for).  
# We make a sub to do this because we use this later to double check set_job_attr
sub check_job_attribute {
	my $get_job_attribute_name = shift;
	my $get_job_attribute_expected_value = shift;
	open( CHIRP, "$condor_chirp_binary get_job_attr $get_job_attribute_name|") or
		die "get_job_attr: condor_chirp did not execute successfully.";
	my @attr_contents = <CHIRP>;
	scalar(@attr_contents)>0 or die "get_job_attr: attribute has zero length."; 
	close CHIRP or die "get_job_attr: could not close pipe."; 
	my $attr_contents_str = join('',@attr_contents); # flatten array
	# Here, we get rid of quotes. TODO: we also get rid of mysterious "s/T" string,
	$attr_contents_str =~ s/^\"?//; # The next two regexes strip away some random 
	$attr_contents_str =~ s/(?:s\/T\W*|\"\W*\n)$//; # cruft that chirp returns
	return $attr_contents_str eq $get_job_attribute_expected_value; 
}
check_job_attribute ("MyType", "Job") or # Here we just check for MyType == Job 
	die "get_job_attr: return value not equal to expected value.";
#####################################################

#####################################################
# This section tests set_job_attr 
my $set_job_attribute_name = "TransferInput";
my $set_job_attribute_new_value = "non_existant_file";
condor_chirp("set_job_attr $set_job_attribute_name $set_job_attribute_new_value") and
	die "set_job_attr: condor_chirp did not execute successfully.";
# Now we use get to check to see if it worked.	
check_job_attribute($set_job_attribute_name,$set_job_attribute_new_value) or
	die "set_job_attr: return value not equal to expected value.";
#####################################################

#####################################################
# This section tests ulog         
my $ulog_test_message = "condor_chirp ulog test";
condor_chirp("ulog \"$ulog_test_message\"")  and
	die "ulog: condor_chirp did not execute successfully.";
exit(0);
