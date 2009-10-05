#!/usr/bin/env perl

# Perl script to prepare and submit the nightly Condor builds in NMI:
# 1) Checks out the latest copy of nmi_tools/condor_nmi_submit.
# 2) Invokes condor_nmi_submit with the right arguments.

# don't edit this file on the build machine... it's in cvs

use Getopt::Long;

# Variables for Getopt::Long::GetOptions
use vars qw/ $opt_git /;

# Parse command-line arguments, currently only one, --git, which turns
# on Git support.
GetOptions('git' => \$opt_git);

# Configuration
$home = '/home/cndrauto/condor';
$log_file = $home .'/nightly.log' . (defined $opt_git ? ".git" : "");
$CVS="/usr/bin/cvs -d /space/cvs/CONDOR_SRC";
$GIT="/prereq/git-1.5.4/bin/git";
$GIT_SRC_ROOT="/space/git/CONDOR_SRC.git";
$GIT_EXT_ROOT="/space/git/CONDOR_EXT.git";
$GIT_DOC_ROOT="/space/git/CONDOR_DOC.git";
$GIT_TSTCNF_ROOT="/space/git/CONDOR_TEST_CNFDTL.git";
$GIT_TSTLRG_ROOT="/space/git/CONDOR_TEST_LRG.git";


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

# We need to make sure the clones are current, since they aren't
# rsync'd constantly anymore
if (defined $opt_git) {
    @roots = ($GIT_SRC_ROOT, $GIT_DOC_ROOT, $GIT_EXT_ROOT, $GIT_TSTCNF_ROOT, $GIT_TSTLRG_ROOT);
    foreach $root (@roots) {
	open(OUT, "$GIT --git-dir=$root fetch 2>&1|")
	    || die "Can't fetch $root: $!\n";
	while (<OUT>) {
	    print LOG;
	}
    }
}

$CNS = "nmi_tools/condor_nmi_submit";

print LOG "Updating copy of nmi_tools, including $CNS\n";
if (defined $opt_git) {
    $checkout_cmd = "$GIT archive --remote=$GIT_SRC_ROOT origin/master nmi_tools | tar xv";
} else {
    $checkout_cmd = "$CVS co -l nmi_tools";
}
open( CVS, "$checkout_cmd 2>&1|" ) || die "Can't execute $checkout_cmd: $!\n";
while (<CVS>) {
  print LOG $_;
}

# don't edit this file on the build machine... it's in cvs

$cns_cmd = "./$CNS --build --nightly --use_externals_cache --clear-externals-cache-weekly --submit-xtests --notify=condor-fw\@cs.wisc.edu --notify-fail-only" . (defined $opt_git ? " --git" : "");

print LOG "Cwd is " . `pwd`;
print LOG "Running $cns_cmd\n";

open( CNS, "$cns_cmd 2>&1|" ) || die "Can't execute $cns_cmd: $!\n";
while (<CNS>) {
  print LOG $_;
}

print LOG "Finished: ". `date` ."\n";
