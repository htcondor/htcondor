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

my $NIGHTLY_IDS_FILE = "/nmi/condor_nightly/test_ids";
my $NMIDIR = "/nmi/run";

# database parameters
my $database = "history";
my $username = "nmi";
my $password = "nmi-nwo-nmi";
my $DB_CONNECT_STR = "DBI:mysql:database=$database;host=localhost";
my $RUN_TABLE = "Run";

#my $PLATFORMS = "x86_rh_9, x86_rh_8.0, x86_rh_7.2, x86_tao_1, sun4u_sol_5.9, sun4u_sol_5.8";
my $PLATFORMS = "x86_rh_9, x86_rh_8.0, x86_rh_7.2, sun4u_sol_5.9, sun4u_sol_5.8";

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
#}
#else {
#    if ( defined($opt_tag) or defined($opt_module) ) {
#        print "You can not have --tag and --nightly at the same time\n";
#        print_usage();
#        exit 1;
#    }
#    %tags = &get_nightlytags();
#}
    %tags = &get_nightlytags();

# Remove the stale $NIGHTLY_IDS_FILE 
if (-f $NIGHTLY_IDS_FILE) {
    unlink ("$NIGHTLY_IDS_FILE") ||
        warn "I was unable to remove the $NIGHTLY_IDS_FILE file. This " . 
             "may lead to more runids being fed to the nightly tests\n";
}

while ( my($tag, $module) = each(%tags) ) {
    my $cmdfile = &generate_cmdfile($tag, $module);
    print "Submitting condor build with tag = $tag, module = $module ...\n";
    run("/nmi/bin/nmi_submit $cmdfile", 0);
    # Sleep for sometime till the records are available in db
    # 10 mins is a good resonable time
    sleep 600;
    write_runid_for_build($tag, $module);
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
    print CMDFILE "prereqs = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, binutils-2.15, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, gcc-2.95.3, coreutils-5.2.1\n";
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
    my ($tag, $module) = @_;

    (my $release = $module) =~ s/_BUILD//;
    $release =~ s/V//; 
    my @versions = split("_", $release);
    my $srcsfile = "condor_srcsfile-$tag";

    my $project = "condor";
    my $project_version = "$versions[0], $versions[1], x";
    my $component = "condor";
    my $component_version = "$versions[0], $versions[1], x";
    my $description = "nightly condor $versions[0].$versions[1] build run";

    my $runid = guess_runid($project, $project_version,
                           $component, $component_version,
                           $description, $srcsfile);
    print "I have guessed that the runid = $runid should be tested\n";

    if ($runid) {
        open (NIGHTLY_IDS, ">>$NIGHTLY_IDS_FILE") ||
            die "Unable to open $NIGHTLY_IDS_FILE for writing $!";
        print NIGHTLY_IDS "$runid $tag $module\n";
        close NIGHTLY_IDS;
    }
}

sub guess_runid() {
    my ($project, $project_version, $component,
        $component_version, $description, $srcsfile) = @_;

    my $matched_runid = 0;
    my $runid = 0;
    my $gid = "";

    my $db = DBI->connect("$DB_CONNECT_STR","$username","$password") ||
             die "Could not connect to database: $!\n";
    my $cmd_str = qq/SELECT runid, gid from $RUN_TABLE WHERE (project='$project' AND project_version='$project_version' AND component='$component' AND component_version='$component_version' AND description='$description') ORDER BY runid DESC/;
    print "$cmd_str\n";

    my $handle = $db->prepare("$cmd_str");
    $handle->execute();
    while ( my $row = $handle->fetchrow_hashref() ) {
        $runid = $row->{'runid'};
        $gid = $row->{'gid'};
       
        print "Checking runid = $runid and gid = $gid\n";
        # Now check if the srcsfile exists in the /nmi/run/$gid
        # if this exists then this is the build and return the runid
        # else keep trying in the reverse order till we have no more
        # ids to verify in which case return 0 as the runid
        if ( ( -d "$NMIDIR/$gid" ) and ( -f "$NMIDIR/$gid/$srcsfile" ) ) {
            $matched_runid = $runid;
            last;
        }
    }
    $handle->finish();
    $db->disconnect;

    return $matched_runid;
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
