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

my $notify;
my %tags;
my $workspace = "/tmp/condor_build." . "$$";

if ( defined($opt_help) ) {
    print_usage();
    exit 0;
}

if ( defined($opt_notify) ) {
    $notify = $opt_notify;
}
else {
    #$notify = "condor-build\@cs.wisc.edu";
    $notify = "wright\@cs.wisc.edu";
}

my $cwd = &getcwd();
mkdir($workspace) || die "Can't create workspace $workspace\n";
chdir($workspace) || die "Can't change workspace $workspace\n";

if (defined($opt_tag) or defined($opt_module) ) {
    print "Sorry --tag and --module not supported yet.\n";
    print_usage();
    exit 1;
}

if ( defined($opt_nightly) ) {
    if ( defined($opt_tag) or defined($opt_module) ) {
        print "You can not have --tag and --nightly at the same time\n";
        print_usage();
        exit 1;
    }
    %tags = &get_nightlytags();
}

while ( my($tag, $module) = each(%tags) ) {
    my $cmdfile = &generate_cmdfile($tag, $module);
    print "Submitting condor build with tag = $tag, module = $module ...\n";
    run("/nmi/bin/nmi_submit $cmdfile", 0);
} 
chdir($cwd);
run("rm -rf $workspace", 0);

exit 0;


sub generate_cmdfile() {
    my ($tag, $module) = @_;

    (my $release = $module) =~ s/_BUILD//;
    $release =~ s/V//; 
    my @versions = split("_", $release);

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
    print CMDFILE "description = nightly condor $versions[0].$versions[1] build run\n";
    print CMDFILE "project = condor\n";
    print CMDFILE "project_release = $versions[0], $versions[1], x\n";
    print CMDFILE "component = condor\n";
    print CMDFILE "component_version = $versions[0], $versions[1], x\n";
    print CMDFILE "sources = $srcsfile\n";
    print CMDFILE "pre_all = nmi_glue/build/pre_all\n";
    print CMDFILE "remote_declare = nmi_glue/build/remote_declare\n";
    print CMDFILE "remote_pre = nmi_glue/build/remote_pre\n";
    print CMDFILE "remote_task = nmi_glue/build/remote_task\n";
    print CMDFILE "remote_post = nmi_glue/build/remote_post\n";
    print CMDFILE "platform_post = nmi_glue/build/platform_post\n";
    print CMDFILE "post_all = nmi_glue/build/post_all\n";
    print CMDFILE "platforms = x86_rh_9, x86_rh_8.0, x86_rh_7.2, sun4u_sol_5.9\n";
    print CMDFILE "prereqs = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, binutils-2.15, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, gcc-2.95.3, coreutils-5.2.1\n";
    print CMDFILE "notify = $notify\n";
    print CMDFILE "priority = 1\n";
    close CMDFILE;

    return $cmdfile;
}

sub get_nightlytags() {
    print "Fetching the build-tag file ...\n";

    my $tag_file = "nwo-build-tags";
    run("wget --tries=1 --non-verbose http://www.cs.wisc.edu/condor/nwo-build-tags");

    my %tags;

    open(TAGS, $tag_file) || die "Can't read nightly tag file $tag_file\n";
    while (<TAGS>) {
        chomp($_);
        my @tag = split /\s/, $_;
        # $tag{'branch'} = module
        $tags{$tag[1]} = $tag[0]; 
    }
    close(TAGS);
    #unlink($tag_file) || warn "Can't delete $tag_file";

    return %tags;
}

sub print_usage() {
    print <<END_USAGE;

--help
This screen

--nightly
This pulls the nightly tags file from http://www.cs.wisc.edu/condor/nwo-build-tags and submits builds for all the tags

--tag
condor source code tag to be fetched from cvs

--module
condor source code module to be fetched from cvs

--notify
List of users to be notified about the results

END_USAGE
}

sub run () {
    my ($cmd, $fatal) = @_;
    my $ret;
    my $output = "";

    # if not specified, the command is fatal
    if (!defined($fatal) or ($fatal != 0 and $fatal != 1)) {
        $fatal = 1;
    }

    print "\n";
    print "#  $cmd\n";

    # run the command
    system("($cmd)  </dev/null 2>&1");
    $ret = $? / 256;

    # should we die here?
    if ($fatal && $ret != 0) {
        print "\n";
        print "FAILED COMMAND: $cmd\n";
        print "RETURN VALUE:  $ret\n";
        print "\n";
        exit(1);
    }

    # return the commands return value
    return $ret;
}
