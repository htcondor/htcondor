#!/usr/bin/env perl

###########################################################################
# 09/25/04: bgietzel : script to run the Condor test suite 
#
# - Get the buildIDs from the nightly test file, and reset the file
# - For each buildID passed 
#    - Generate the submit file and helper files
#    - run nmi_submit 
###########################################################################

#use strict;
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

#format of test_ids file
#109 BUILD-V6_6-branch-2004-9-27 V6_6_BUILD

# database parameters
my $database = "history";
my $username = "nmi";
my $password = "nmi-nwo-nmi";
my $DB_CONNECT_STR = "DBI:mysql:database=$database;host=localhost";
my $RUN_TABLE = "Run";
my $TASK_TABLE = "Task";

my $notify;
my %ids;
my @platforms;
my $gid;
my $workspace = "/tmp/condor_test." . "$$";

if ( defined($opt_help) ) {
    print_usage();
    exit 0;
}

if ( defined($opt_notify) ) {
    $notify = "$opt_notify";
}
else {
    $notify = "condor-build\@cs.wisc.edu";
    #$notify = "bgietzel\@cs.wisc.edu";
}

my $cwd = &getcwd();
my $NIGHTLY_IDS_FILE = "$cwd/test_ids";

mkdir($workspace) || die "Can't create workspace $workspace\n";
chdir($workspace) || die "Can't change workspace $workspace\n";

if ( defined($opt_tag) or defined($opt_module) ) {
    print "Sorry --tag and --module not supported yet.\n";
    print_usage();
    exit 1;
}
#if ( defined($opt_nightly) ) {
else {
    if ( defined($opt_tag) or defined($opt_module) ) {
        print "You can not have --tag and --nightly at the same time\n";
        print_usage();
        exit 1;
    }
    %ids = &get_nightlyids();
}

# Check that the $NIGHTLY_IDS_FILE exists
(-f $NIGHTLY_IDS_FILE)  ||  die "$NIGHTLY_IDS_FILE does not exist, no tests run\n";


while ( my($id, $tag_module_string) = each(%ids) ) {
    @platforms = &get_platforms($id);
    $gid = &get_gid($id);
    my $cmdfile = &generate_cmdfile($id, $tag_module_string, @platforms);
    print "Submitting condor test suite run for build $id ($tag_module_string) ...\n";
    run("/nmi/bin/nmi_submit $cmdfile", 0);
} 
chdir($cwd);
#run("rm -rf $workspace", 0);

exit 0;


sub generate_cmdfile() {
    my ($id, $tag_module_string, @platforms) = @_;
    
    my @platforms = @platforms;

    print "tagmodule string is $tag_module_string\n";
    #get tag and module
    my ($tag, $module) = split(/ /, $tag_module_string); 
    print "++tag is $tag\n";
    print "++module is $module\n";
 
    my $release = $tag;
    $release =~ s/BUILD-//;
    my $desc = $release;
    $release =~ s/-branch-.*//;
    $release =~ s/V//;
    if( $release =~ /(\d+)(\D*)_(\d+)(\D*)_?(\d+)?(\D*)/ ) {
        $versions[0] = $1;
        $versions[1] = $3;
	if( $5 ) {
            $versions[2] = $5;
        } else {
            $versions[2] = "x";
	}
    }
    my $vers_string = "$versions[0], $versions[1], $versions[2]";

    my $cmdfile = "condor_cmdfile-$tag";
    my $srcsfile = "condor_srcsfile-$tag";
    my $gluefile = "condor_test.src";
    my $runidfile = "input_build_runid.src";

    # generate the test glue file - may be symlinked eventually
    open(GLUEFILE, ">$gluefile") || die "Can't open $gluefile for writing.";
    print GLUEFILE "method = cvs\n";
    print GLUEFILE "cvs_root = :ext:cndr-cvs\@chopin.cs.wisc.edu:/p/condor/repository/CONDOR_SRC\n";
    print GLUEFILE "cvs_server = /afs/cs.wisc.edu/p/condor/public/bin/auth-cvs\n";
    print GLUEFILE "cvs_rsh = /nmi/scripts/ssh_no_x11\n";
    print GLUEFILE "cvs_tag = $tag\n"; 
    print GLUEFILE "cvs_module = nmi_glue/test\n";
    close GLUEFILE;

    # generate the runid input file
    open(RUNIDFILE, ">$runidfile") || die "Can't open $runidfile for writing.";
    print RUNIDFILE "method = nmi\n";
    print RUNIDFILE "input_runids = $id\n";
    print RUNIDFILE "untar_results = false\n";     
    close RUNIDFILE;

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
    print CMDFILE "description = $desc\n";
    print CMDFILE "run_type = test\n";
    print CMDFILE "project = condor\n";
    print CMDFILE "project_release = $vers_string\n";
    print CMDFILE "component = condor\n";
    print CMDFILE "component_version = $vers_string\n";
    print CMDFILE "sources = $runidfile, $gluefile\n";
    print CMDFILE "pre_all = nmi_glue/test/pre_all\n";
    print CMDFILE "platform_pre = nmi_glue/test/platform_pre\n";
    print CMDFILE "remote_pre_declare = nmi_glue/test/remote_pre_declare\n";
    print CMDFILE "remote_declare = nmi_glue/test/remote_declare\n";
    # select scope of testsuite run 
    # all takes 30 minutes
    print CMDFILE "remote_declare_args = all\n";
    print CMDFILE "remote_pre = nmi_glue/test/remote_pre\n";
    print CMDFILE "remote_task = nmi_glue/test/remote_task\n";
    print CMDFILE "remote_post = nmi_glue/test/remote_post\n";
    print CMDFILE "platforms = ";
    foreach $platform (@platforms) { 
        print CMDFILE "$platform,";
    }
    print CMDFILE "\n";

    # global prereqs
    print CMDFILE "prereqs = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, coreutils-5.2.1, binutils-2.15\n";

    # platform-specific prereqs
    print CMDFILE "prereqs_sun4u_sol_5.9 = gcc-2.95.3\n";
    print CMDFILE "prereqs_sun4u_sol_5.8 = gcc-2.95.3\n";

    print CMDFILE "notify = $notify\n";
    print CMDFILE "priority = 1\n";
    close CMDFILE;

    return $cmdfile;
}

sub get_nightlyids() {
    print "Getting build ids and source to run against ...\n";

    my %ids;

    open(IDS, "<", "$NIGHTLY_IDS_FILE") || die "Can't read nightly ids file $NIGHTLY_IDS_FILE\n";
    while (<IDS>) {
        chomp($_);
        my @id = split /\s/, $_;
        $ids{$id[0]} = "$id[1]" . " " . "$id[2]"; 
    }
    # truncate file
    #seek(IDS, 0, 0) || die "can't seek to $NIGHTLY_IDS_FILE\n"; 
    #print IDS "" || die "can't print to $NIGHTLY_IDS_FILE\n";
    #truncate(IDS, tell(IDS)) || die "can't truncate $NIGHTLY_IDS_FILE\n";  
    close(IDS) || die "can't close $NIGHTLY_IDS_FILE\n";

    return %ids;
}

sub get_gid() {
    my ($runid) = @_;

    my $db = DBI->connect("$DB_CONNECT_STR","$username","$password") ||
             die "Could not connect to database: $!\n";
    my $cmd_str = qq/SELECT gid from $RUN_TABLE WHERE runid='$runid'/;
    print "$cmd_str\n";

    my $handle = $db->prepare("$cmd_str");
    $handle->execute();
    while ( my $row = $handle->fetchrow_hashref() ) {
        $gid = $row->{'gid'};
        print "gid = $gid\n";
    }
    $handle->finish();
    $db->disconnect;

    return $gid;
}

sub get_platforms() {
    my ($runid) = @_;

    my @platforms = ();
    my $db = DBI->connect("$DB_CONNECT_STR","$username","$password") ||
             die "Could not connect to database: $!\n";
    my $cmd_str = qq/SELECT DISTINCT platform from $TASK_TABLE WHERE runid='$runid'/;
    print "$cmd_str\n";

    my $handle = $db->prepare("$cmd_str");
    $handle->execute();
    while ( my $row = $handle->fetchrow_hashref() ) {
        $platform = $row->{'platform'};
        push(@platforms, $platform);
    }
    $handle->finish();    
    $db->disconnect;

    # strip out local platform entry
    @platforms = grep !/local/, @platforms;

    return @platforms;
}

sub print_usage() {
    print <<END_USAGE;

--help
This screen

--nightly (default)
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
