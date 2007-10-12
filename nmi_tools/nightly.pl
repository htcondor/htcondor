#!/usr/bin/env perl

# Perl script to prepare and submit the nightly Condor builds in NMI:
# 1) Checks out the latest copy of nmi_tools/condor_nmi_submit.
# 2) Invokes condor_nmi_submit with the right arguments.

# Configuration
$home = '/home/cndrauto';
$cvs_dir = $home .'/cvs';
$log_dir = $home .'/log';
$log_file = $log_dir .'/nightly.log';
$CVS="/usr/bin/cvs -d /space/cvs/CONDOR_SRC";


# Open log file and setup autoflushing
unlink( "$log_file.old" );
rename( $log_file, "$log_file.old" );
open( LOG, ">$log_file" ) || die "Can't open $log_file: $!\n";
$save_fh = select(LOG);
$| = 1;
select($save_fh);


# Define a new version of die
$SIG{__DIE__} = sub { 
    my $message = shift; 
    print LOG "ERROR: $message"; 
    exit 1;
}; 


### Real work begins

print LOG "Starting: ". `date` ."\n";

chdir $cvs_dir || die "Can't chdir($cvs_dir): $!\n";

print LOG "Updating copy of condor_nmi_submit\n";
$checkout_cmd = "$CVS co nmi_tools/condor_nmi_submit";
open( CVS, "$checkout_cmd 2>&1|" ) || die "Can't execute $checkout_cmd: $!\n";
while (<CVS>) {
  print LOG $_;
}

print LOG "Running condor_nmi_submit\n";

$cns_cmd = "./nmi_tools/condor_nmi_submit --build --nightly --notify=condor-build@cs.wisc.edu";

open( CNS, "$cns_cmd 2>&1|" ) || die "Can't execute $cns_cmd: $!\n";
while (<CNS>) {
  print LOG $_;
}

print LOG "Finished: ". `date` ."\n";
