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
use DBI;
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

my $NMIDIR = "/nmi/run";

# database parameters
my $database = "history";
my $username = "nmi";
my $password = "nmi-nwo-nmi";
my $DB_CONNECT_STR = "DBI:mysql:database=$database;host=localhost";
my $RUN_TABLE = "Run";

my $PLATFORMS = "x86_rh_9, x86_rh_8.0, x86_rh_7.2, x86_tao_1, sun4u_sol_5.9, sun4u_sol_5.8, ppc_aix_5.2, ppc_macosx_7.5.0, x86_64_sles_8";
#my $PLATFORMS = "x86_rh_9, x86_rh_8.0, x86_rh_7.2, sun4u_sol_5.9, sun4u_sol_5.8, ppc_aix_5.2";
#my $PLATFORMS = "ppc_aix_5.2";

my $notify;
my %tags;
my $workspace = "/tmp/condor_build." . "$$";

if ( defined($opt_help) ) {
    print_usage();
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
    print_usage();
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

    # global prereqs
    print CMDFILE "prereqs = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, coreutils-5.2.1\n";

    # platform-specific prereqs
    print CMDFILE "prereqs_sun4u_sol_5.9 = gcc-2.95.3, binutils-2.14\n";
    print CMDFILE "prereqs_sun4u_sol_5.8 = gcc-2.95.3, binutils-2.14\n";
    print CMDFILE "prereqs_x86_rh_9 = binutils-2.15\n";
    print CMDFILE "prereqs_x86_rh_8.0 = binutils-2.15\n";
    print CMDFILE "prereqs_x86_rh_7.2 = binutils-2.15\n";
    print CMDFILE "prereqs_ppc_aix_5.2 = binutils-2.15\n";

    print CMDFILE "notify = $notify\n";
    print CMDFILE "priority = 1\n";
    print CMDFILE "run_type = build\n";
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
        $tags{$tag[1]} = $tag[0]; 
    }
    close(TAGS);

    return %tags;
}

sub write_runid_for_build() {
    my ($nmirundir, $tag, $module) = @_;

    my @toks = split("/", $nmirundir);
    my $gid = $toks[3];

    my $runid = get_runid($gid);
    print "I have guessed that the runid = $runid should be tested\n";

    my $BUILD_INFO_FILE = "$nmirundir/userdir/common/test_ids"; 
    print "BUILD_INFO_FILE = $BUILD_INFO_FILE\n";
    if ($runid) {
        open (INFO_FILE, ">>$BUILD_INFO_FILE") ||
            die "Unable to open $BUILD_INFO_FILE for writing $!";
        print INFO_FILE "$runid $tag $module\n";
        close INFO_FILE;
    }
}

sub get_runid() {
    my ($gid) = @_;
    my $runid = "";

    my $db = DBI->connect("$DB_CONNECT_STR","$username","$password") ||
             die "Could not connect to database: $!\n";
    my $cmd_str = qq/SELECT runid from $RUN_TABLE WHERE gid='$gid'/;
    print "$cmd_str\n";

    my $handle = $db->prepare("$cmd_str");
    $handle->execute();
    while ( my $row = $handle->fetchrow_hashref() ) {
        $runid = $row->{'runid'};
        last;
    }
    $handle->finish();
    $db->disconnect;

    return $runid;
}

sub print_usage() {
    print <<END_USAGE;

--help
This screen

--nightly (default)
This pulls the nightly tags file from http://www.cs.wisc.edu/condor/nwo-build-tags and submits builds for all the tags

--tag (not supported yet)
condor source code tag to be fetched from cvs

--module (not supported yet)
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
