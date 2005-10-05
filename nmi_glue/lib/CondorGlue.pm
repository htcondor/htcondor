######################################################################
# Perl module to handle a lot of shared functionality for the Condor
# build and test "glue" scripts for use with the NMI-NWO framework.
#
# Originally written by Derek Wright <wright@cs.wisc.edu> 2004-12-30
# $Id: CondorGlue.pm,v 1.1.4.16 2005-10-05 06:32:44 wright Exp $
#
######################################################################

package CondorGlue;

use Cwd;
use DBI;
use File::Basename;

use Getopt::Long;
use vars qw/ $opt_help $opt_nightly $opt_tag $opt_module $opt_notify
     $opt_platforms $opt_externals $opt_without_tests /;

# database parameters
my $database = "history";
my $username = "nmi";
my $password = "nmi-nwo-nmi";
my $DB_CONNECT_STR = "DBI:mysql:database=$database;host=localhost";
my $RUN_TABLE = "Run";

# Location of the nightly build tag file
my $tag_file_url = "http://www.cs.wisc.edu/condor/nwo-build-tags";

my $init_cwd;
my $workspace;
my $notify;
my $platforms;
my $ext_mod;
my %tags;
our $no_tests;


sub Initialize
{
    my ($type) = @_;
    
    $ENV{PATH} = "/nmi/bin:/usr/local/condor/bin:/usr/local/condor/sbin:"
        . $ENV{PATH};

    $workspace = "/tmp/condor_$type." . "$$" . "." . time;

    $init_cwd = &getcwd();
    mkdir($workspace) || die "Can't create workspace $workspace: $!\n";
    chdir($workspace) || die "Can't chdir($workspace): $!\n";
}


sub ProcessOptions
{
    my $default_platforms = shift;

    GetOptions(
           'help'          => $opt_help,
           'nightly'       => $opt_nightly,
           'tag=s'         => $opt_tag,
           'module=s'      => $opt_module,
           'externals=s'   => $opt_externals,
           'notify=s'      => $opt_notify,
           'platforms=s'   => $opt_platforms,
	   'without-tests' => $opt_without_tests
    );

    if( defined($opt_help) ) {
        printBuildUsage( 0 );
    }

    if( defined($opt_platforms) ) {
        $platforms = "$opt_platforms";
    } else {
        $platforms = "$default_platforms";
    }

    if( defined($opt_notify) ) {
        $notify = "$opt_notify";
    } else {
        $notify = "condor-build\@cs.wisc.edu";
    }

    if ( defined($opt_tag) or defined($opt_module) ) {
        print "You specified --tag=$opt_tag and --module=$opt_module\n";
	if( $opt_module !=~ /_BUILD$/ ) {
	    print STDERR "warning: module name should end in _BUILD to include externals\n";
	}
        if ( defined ($opt_tag) and defined($opt_module) ) {
            $tags{"$opt_tag"} = $opt_module;
        } else {
            print "ERROR: You need to specify both --tag and --module\n";
            printBuildUsage( 1 );
        }
    }
    elsif ( defined($opt_nightly) ) {
        getNightlyTags();
    }
    else {
        print "You need to have --tag with --module or --nightly\n";
        printBuildUsage( 1 );
    }

    if( defined($opt_externals) ) {
        if( defined($opt_nightly) ) {
            print "ERROR: You cannot use --externals with --nightly\n";
            printBuildUsage( 1 );
        }
        $ext_mod = "$opt_externals";
    }

    if( defined($opt_without_tests) ) {
	$no_tests = 1;
    }

}


sub buildLoop
{
    my $func_ref = shift;
    while ( my($tag, $module) = each(%tags) ) {
        my $cmdfile = generateBuildFile($tag,$module,$func_ref);
        print "Submitting condor build with tag = $tag, module = $module\n";
        my $output_str=`/nmi/bin/nmi_submit $cmdfile`;
        my $status = $?;
        if( not $status ) {
            # Sleep for sometime till the records are available in db
            # 30sec is a good resonable time
            sleep 30;
            my @lines = split("\n", $output_str);
            writeRunidForBuild($lines[15], $tag, $module);
        } else {
            print "nmi_submit failed\n";
        }
    } 
    chdir( $init_cwd );
    run( "rm -rf $workspace", 0 );
    exit 0;
}


sub generateBuildFile
{
    my ($tag, $module, $custom_func_ref) = @_;

    my $cmdfile = "condor_cmdfile-$tag";
    my $srcsfile = "condor_srcsfile-$tag";
    my $extfile = "condor_extfile-$tag";

    # Generate the source code file
    makeFetchFile( $srcsfile, $module, $tag );

    if( $ext_mod ) {
        CondorGlue::makeFetchFile( $extfile, $ext_mod );
    }

    # Generate the cmdfile
    open(CMDFILE, ">$cmdfile") || die "Can't open $cmdfile for writing: $!\n";

    printIdentifiers( *CMDFILE, $tag );
    printPlatforms( *CMDFILE );
    print CMDFILE "run_type = build\n";

    # define inputs
    print CMDFILE "inputs = $srcsfile";
    if( $ext_mod ) {
        print CMDFILE ", $extfile\n";
    } else {
        print CMDFILE "\n";
    }

    &$custom_func_ref( *CMDFILE, $tag, $module );

    close CMDFILE;

    return $cmdfile;
}


# Hash table to define the prereqs.  We no longer use the global
# "prereqs" setting in the NMI description file for our jobs, since we
# have platforms (e.g. HPUX 10) that can't share prereqs with the
# others.  So, besides the special cases, all platforms get the value
# in this hashtable for "global", followed by whatever is in the
# corresponding "<platform>" entry.

my %prereqs = (
  "global"         => "perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4",
  "x86_rh_7.2"     => "binutils-2.15",
  "x86_rh_8.0"     => "binutils-2.15",
  "x86_rh_9"       => "binutils-2.15",
  "sun4u_sol_5.9"  => "gcc-2.95.3, binutils-2.15",
  "sun4u_sol_5.8"  => "gcc-2.95.3, binutils-2.15",
  "alpha_osf_V5.1" => "gcc-2.95.3, binutils-2.15",
  "alpha_rh_7.2"   => "gcc-2.96, binutils-2.15",
  "ppc_aix_5.2"    => "vac-6, vacpp-6",
  "irix_6.5"       => "egcs-2.91.66"
);


sub printPrereqs
{
    my $fh = shift;

    # global prereqs should be nothign because of windows and
    # (temporarily) HPUX
    #print $fh "prereqs = $global_prereqs\n";

    # platform-specific prereqs
    foreach $platform ( "x86_rh_7.2", "x86_rh_8.0", "x86_rh_9", 
			"sun4u_sol_5.9", "sun4u_sol_5.8",
			"ppc_aix_5.2", "alpha_osf_V5.1", "alpha_rh_7.2", "irix_6.5" )
    {
	print $fh "prereqs_$platform = $prereqs{'global'}, "
	    . "$prereqs{$platform}\n";
    }

    # HPUX 10 is (temporarily) evil, since we're not going to really
    # set it up as a full-fledged NWO platform.  instead, we're just
    # using this "everything-1.0.0" prereq hack...
    print $fh "prereqs_hppa_hpux_B.10.20 = everything-1.0.0\n";
}


sub printTestingPrereqs
{
    my $fh = shift;

    # global prereqs should be nothing (temporarily) because of hpux
    
    # platform-specific prereqs
    # we include everything from the build prereqs, plus we add java
    # for the java test suite...
    foreach $platform ( "x86_rh_7.2", "x86_rh_8.0", "x86_rh_9", 
			"sun4u_sol_5.9", "sun4u_sol_5.8",
			"ppc_aix_5.2" )
    {
	print $fh "prereqs_$platform = $prereqs{'global'}, "
	    . "$prereqs{$platform}, java-1.4.2_05\n";
    }

    # HACK: irix_6.5 and alpha platforms don't have java yet, so we
    # can't use the real loop above...
    foreach $platform ( "irix_6.5", "alpha_osf_V5.1", "alpha_rh_7.2" )
    {
	print $fh "prereqs_$platform = $prereqs{'global'}, "
	    . "$prereqs{$platform}\n";
    }

    # HPUX 10 is evil, see above...
    print $fh "prereqs_hppa_hpux_B.10.20 = everything-1.0.0\n";
}


sub printIdentifiers
{
    my $fh = shift;
    my $tag = shift;

    my $desc;
    my @vers;

    ($desc, @vers) = CondorGlue::parseTag( $tag );
    my $vers_string = join( ',', @vers );

    print $fh "description = $desc\n";
    print $fh "project = condor\n";
    print $fh "project_release = $vers_string\n";
    print $fh "component = condor\n";
    print $fh "component_version = $vers_string\n";
    print $fh "notify = $notify\n";
    print $fh "private_web_users = condor-team\n";
    print $fh "priority = 1\n";
}


sub printPlatforms
{
    my $fh = shift;
    print $fh "platforms = $platforms\n";
}


sub parseTag
{
    my $tag = shift;
    my $desc;
    my @vers;

    $tag =~ s/BUILD-//;
    my $desc = $tag;
    $tag =~ s/-branch-.*//;
    $tag =~ s/V//;
    if( $tag =~ /(\d+)(\D*)_(\d+)(\D*)_?(\d+)?(\D*)/ ) {
        $vers[0] = $1;
        $vers[1] = $3;
        if( $5 ) {
            $vers[2] = $5;
        } else {
            $vers[2] = "x";
        }
    }
    return ( $desc, @vers );
}


sub writeRunidForBuild {
    my ($nmirundir, $tag, $module) = @_;

    my @toks = split("/", $nmirundir);
    my $gid = $toks[3];

    my $runid = getRunid($gid);
    print "I have guessed that the runid = $runid should be tested\n";

    my $BUILD_INFO_FILE = "$nmirundir/userdir/common/test_ids"; 
    print "BUILD_INFO_FILE = $BUILD_INFO_FILE\n";
    if ($runid) {
        open (INFO_FILE, ">>$BUILD_INFO_FILE") ||
            die "Unable to open $BUILD_INFO_FILE for writing: $!\n";
        print INFO_FILE "$runid $tag $module\n";
        close INFO_FILE;
    }
}

sub getRunid {
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


sub run
{
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


sub getNightlyTags
{
    my $tag_file = basename($tag_file_url);

    print "Fetching $tag_file ... \n";
    run( "wget --tries=1 $tag_file_url" );

    open(TAGS, $tag_file) || 
        die "Can't read nightly tag file $tag_file: $!\n";
    while (<TAGS>) {
        chomp($_);
        my @tag = split /\s/, $_;
        $tags{$tag[1]} = $tag[0]; 
    }
    close(TAGS);
}


sub printBuildUsage
{
    my $exit_code = shift;
    print <<END_USAGE;

--help
This screen

--nightly (default)
This pulls the nightly tags file from http://www.cs.wisc.edu/condor/nwo-build-tags and submits builds for all the tags

--tag
condor source code tag to be fetched from cvs

--module
condor source code module to be fetched from cvs (note: should end in _BUILD)

--notify
List of users to be notified about the results

--platforms
List of platforms to build (default: all currently known-working)

--without-tests
Submit a build job such that it will NOT submit test runs as each
platform completes

END_USAGE

    chdir($init_cwd);
    run("rm -rf $workspace", 0);
    exit $exit_code;
}


sub makeFetchFile
{
    my ( $file, $module, $tag ) = @_;

    open( FILE, ">$file" ) || die "Can't open $file for writing: $!\n";
    print FILE "method = cvs\n";
    print FILE "cvs_root = /space/cvs/CONDOR_SRC\n";
#    print FILE "cvs_root = :ext:cndr-cvs\@chopin.cs.wisc.edu:/p/condor/repository/CONDOR_SRC\n";
#    print FILE "cvs_server = /afs/cs.wisc.edu/p/condor/public/bin/auth-cvs\n";
#    print FILE "cvs_rsh = /nmi/scripts/ssh_no_x11\n";
    print FILE "cvs_module = $module\n";
    if( $tag ) {
        print FILE "cvs_tag = $tag\n";
    }
    close FILE;
}


1;

