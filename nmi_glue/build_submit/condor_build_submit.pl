#!/usr/bin/env perl

###########################################################################
# NOTE: This scripts builds condor 
#
# 1. Get the buildtags from the nighlty build-tag on the CSL machine or 
#    from the command line arguments
# 2. For each tag passed 
#    - Generate the condor_build.desc and sources files
#    - run nmi_submit
###########################################################################

use strict;
use File::Basename;
my $LIBDIR;
BEGIN {
    my $dir1 = dirname($0);
    my $dir2 = dirname($dir1);
    if( $dir1 eq $dir2 ) {
	$LIBDIR = $dir2 . "/../lib";
    } else {
	$LIBDIR = $dir2 . "/lib";
    }
}
use lib "$LIBDIR";
use CondorGlue;

CondorGlue::Initialize();

my $default_platforms = 
    "x86_rh_9, " .
    "x86_rh_8.0, " .
    "x86_rh_7.2, " .
    "sun4u_sol_5.9, " .
    "sun4u_sol_5.8, " .
    "ppc_aix_5.2, " .
    "ppc_macosx";
CondorGlue::ProcessOptions( $default_platforms );

# will exit when finished
CondorGlue::buildLoop( \&generateCmdFile ); 


sub generateCmdFile() {
    my ($tag, $module) = @_;

    my $cmdfile = "condor_cmdfile-$tag";
    my $srcsfile = "condor_srcsfile-$tag";

    # Generate the source code file
    CondorGlue::makeFetchFile( $srcsfile, $module, $tag );

    # Generate the cmdfile
    open(CMDFILE, ">$cmdfile") || die "Can't open $cmdfile for writing: $!\n";

    CondorGlue::printIdentifiers( *CMDFILE, $tag );
    CondorGlue::printPlatforms( *CMDFILE );
    print CMDFILE "run_type = build\n";

    # define inputs
    print CMDFILE "inputs = $srcsfile\n";

    # define the glue scripts we want
    print CMDFILE "pre_all = nmi_glue/build/pre_all\n";
    print CMDFILE "remote_declare = nmi_glue/build/remote_declare\n";
    print CMDFILE "remote_pre = nmi_glue/build/remote_pre\n";
    print CMDFILE "remote_task = nmi_glue/build/remote_task\n";
    print CMDFILE "remote_post = nmi_glue/build/remote_post\n";
    print CMDFILE "platform_post = nmi_glue/build/platform_post\n";
    print CMDFILE "post_all = nmi_glue/build/post_all\n";
    print CMDFILE "post_all_args = $tag $module\n";

    # misc administrative stuff
    CondorGlue::printPrereqs( *CMDFILE );

    close CMDFILE;

    return $cmdfile;
}

