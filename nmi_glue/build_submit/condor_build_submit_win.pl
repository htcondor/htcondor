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

my $default_platforms = "winnt_5.1";
CondorGlue::ProcessOptions( $default_platforms );

# will exit when finished
CondorGlue::buildLoop( \&customizeCmdFile ); 

sub customizeCmdFile() {
    my ($fh, $tag, $module) = @_;

    # define the glue scripts we want
#    print CMDFILE "pre_all = nmi_glue/build/pre_all\n";
#    print CMDFILE "remote_declare = nmi_glue/build/remote_declare\n";
#    print CMDFILE "remote_pre = nmi_glue/build/remote_pre\n";
    print CMDFILE "remote_task = nmi_glue\\build\\perl.bat\n";
    print CMDFILE "remote_task_args = nmi_glue\\build\\remote_task.win\n";
    print CMDFILE "remote_post = nmi_glue\\build\\perl.bat\n";
    print CMDFILE "remote_post_args = nmi_glue\\build\\remote_post\n";
#    print CMDFILE "platform_post = nmi_glue/build/platform_post\n";
#    print CMDFILE "post_all = nmi_glue/build/post_all\n";
#    print CMDFILE "post_all_args = $tag $module\n";

    # misc administrative stuff
    # global prereqs
# TODO -- should add cygwin here
#    print CMDFILE "prereqs = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, coreutils-5.2.1\n";
}
