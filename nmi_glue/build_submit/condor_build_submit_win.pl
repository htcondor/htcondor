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
use File::Copy;
use Cwd;
use Getopt::Long;
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

use vars qw/ $opt_help $opt_nightly $opt_tag $opt_module $opt_notify/;

$ENV{PATH} = "/nmi/bin:/usr/local/condor/bin:/usr/local/condor/sbin:"
             . $ENV{PATH};

GetOptions (
    'help'            => $opt_help,
    'nightly'         => $opt_nightly,
    'tag'             => $opt_tag,
    'module'          => $opt_module,
    'notify'          => $opt_notify,
);

my $PLATFORMS = "winnt_5.1";

my $notify;
my %tags;
my $workspace = "/tmp/condor_build." . "$$";

if ( defined($opt_help) ) {
    CondorGlue::printBuildUsage();
    exit 0;
}

if ( defined($opt_notify) ) {
    $notify = "$opt_notify";
}
else {
    $notify = "condor-build\@cs.wisc.edu";
}

my $cwd = &getcwd();
mkdir($workspace) || die "Can't create workspace $workspace\n";
chdir($workspace) || die "Can't change workspace $workspace\n";

if ( defined($opt_tag) or defined($opt_module) ) {
    print "Sorry --tag and --module not supported yet.\n";
    CondorGlue::printBuildUsage();
    exit 1;
}
#if ( defined($opt_nightly) ) {
#    %tags = &get_nightlytags();
#}
#else {
#    if ( defined($opt_tag) or defined($opt_module) ) {
#        print "You can not have --tag and --nightly at the same time\n";
#        print_usage();
#        exit 1;
#    }
#}
    %tags = &get_nightlytags();

while ( my($tag, $module) = each(%tags) ) {
    my $cmdfile = &generate_cmdfile($tag, $module);
    print "Submitting condor build with tag = $tag, module = $module ...\n";
    my $output_str=`/nmi/bin/nmi_submit $cmdfile`;
    my $status = $?;
    if (not $status) {
        # Sleep for sometime till the records are available in db
        # 30sec is a good resonable time
        sleep 30;
        my @lines = split("\n", $output_str);
        write_runid_for_build($lines[15], $tag, $module);
    }
    else {
        print "nmi_submit failed\n";
    }
} 
chdir($cwd);
run("rm -rf $workspace", 0);

exit 0;


sub generate_cmdfile() {
    my ($tag, $module) = @_;

    my $cmdfile = "condor_cmdfile-$tag";
    my $srcsfile = "condor_srcsfile-$tag";

    # Generate the source code file
    CondorGlue::makeFetchFile( $srcsfile, $module, $tag );

    # Generate the cmdfile
    open(CMDFILE, ">$cmdfile") || die "Can't open $cmdfile for writing.";

    CondorGlue::printIdentifiers( *CMDFILE, $tag );
    print CMDFILE "run_type = build\n";

    # define inputs
    print CMDFILE "inputs = $srcsfile\n";

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
    print CMDFILE "platforms = $PLATFORMS\n";
    print CMDFILE "notify = $notify\n";
    # global prereqs
# TODO -- should add cygwin here
#    print CMDFILE "prereqs = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, coreutils-5.2.1\n";

    close CMDFILE;

    return $cmdfile;
}
