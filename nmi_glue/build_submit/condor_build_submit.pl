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
    'help'              => $opt_help,
    'nightly'           => $opt_nightly,
    'tag=s'             => $opt_tag,
    'module=s'          => $opt_module,
    'notify=s'          => $opt_notify,
);

my $NMIDIR = "/nmi/run";

my $PLATFORMS = "x86_rh_9, x86_rh_8.0, x86_rh_7.2, sun4u_sol_5.9, sun4u_sol_5.8, ppc_aix_5.2";

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

if ( defined($opt_tag) or defined($opt_module) ) {
    print "You specified --tag=$opt_tag and --module-$opt_module\n";
    if ( defined ($opt_tag) and defined($opt_module) ) {
        $tags{"$opt_tag"} = $opt_module;
    } else {
        print "ERROR: You need to specify both --tag and --module\n";
	CondorGlue::printBuildUsage();
        exit 1;
    }
}
elsif ( defined($opt_nightly) ) {
    %tags = &CondorGlue::getNightlyTags();
}
else {
    print "You need to have --tag with --module or --nightly\n";
    CondorGlue::printBuildUsage();
    exit 1;
}


my $cwd = &getcwd();
mkdir($workspace) || die "Can't create workspace $workspace\n";
chdir($workspace) || die "Can't change workspace $workspace\n";

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
	CondorGlue::writeRunidForBuild($lines[15], $tag, $module);
    }
    else {
        print "nmi_submit failed\n";
    }
} 
chdir($cwd);
CondorGlue::run("rm -rf $workspace", 0);

exit 0;


sub generate_cmdfile() {
    my ($tag, $module) = @_;

    my $cmdfile = "condor_cmdfile-$tag";
    my $srcsfile = "condor_srcsfile-$tag";

    # Generate the source code file
    open(SRCSFILE, ">$srcsfile") || die "Can't open $srcsfile for writing.";
    print SRCSFILE "method = cvs\n";
    print SRCSFILE "cvs_root = :ext:cndr-cvs\@chopin.cs.wisc.edu:/p/condor/repository/CONDOR_SRC\n";
    print SRCSFILE "cvs_server = /afs/cs.wisc.edu/p/condor/public/bin/auth-cvs\n";
    print SRCSFILE "cvs_rsh = /nmi/scripts/ssh_no_x11\n";
    print SRCSFILE "cvs_tag = $tag\n";
    print SRCSFILE "cvs_module = $module\n";
    close SRCSFILE;


    # Generate the cmdfile
    open(CMDFILE, ">$cmdfile") || die "Can't open $cmdfile for writing.";

    CondorGlue::printIdentifiers( *CMDFILE, $tag );

    print CMDFILE "inputs = $srcsfile\n";
    print CMDFILE "pre_all = nmi_glue/build/pre_all\n";
    print CMDFILE "remote_declare = nmi_glue/build/remote_declare\n";
    print CMDFILE "remote_pre = nmi_glue/build/remote_pre\n";
    print CMDFILE "remote_task = nmi_glue/build/remote_task\n";
    print CMDFILE "remote_post = nmi_glue/build/remote_post\n";
    print CMDFILE "platform_post = nmi_glue/build/platform_post\n";
    print CMDFILE "post_all = nmi_glue/build/post_all\n";
    print CMDFILE "post_all_args = $tag $module\n";
    print CMDFILE "platforms = $PLATFORMS\n";

    CondorGlue::printPrereqs( *CMDFILE );

    print CMDFILE "notify = $notify\n";
    print CMDFILE "priority = 1\n";
    print CMDFILE "run_type = build\n";
    close CMDFILE;

    return $cmdfile;
}

