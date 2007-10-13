#!/usr/bin/env perl

# Perl script to prepare and submit the nightly Condor builds in NMI:
# 1) Checks out the latest copy of nmi_tools/condor_nmi_submit.
# 2) Invokes condor_nmi_submit with the right arguments.

# Configuration
$home = '/home/cndrauto/condor';
$log_file = $home .'/nightly.log';
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

chdir $home || die "Can't chdir($home): $!\n";

$CNS = "nmi_tools/condor_nmi_submit";

print LOG "Updating copy of $CNS\n";
$checkout_cmd = "$CVS co nmi_tools -l";
open( CVS, "$checkout_cmd 2>&1|" ) || die "Can't execute $checkout_cmd: $!\n";
while (<CVS>) {
  print LOG $_;
}

print LOG "Running condor_nmi_submit\n";

$cns_cmd = "./$CNS --build --nightly --use_externals_cache --notify=condor-build@cs.wisc.edu";

open( CNS, "$cns_cmd 2>&1|" ) || die "Can't execute $cns_cmd: $!\n";
while (<CNS>) {
  print LOG $_;
}

print LOG "Finished: ". `date` ."\n";
