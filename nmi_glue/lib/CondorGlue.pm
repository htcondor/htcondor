######################################################################
# Perl module to handle a lot of shared functionality for the Condor
# build and test "glue" scripts for use with the NMI-NWO framework.
#
# Originally written by Derek Wright <wright@cs.wisc.edu> 2004-12-30
# $Id: CondorGlue.pm,v 1.1.2.4 2004-12-31 06:28:50 wright Exp $
#
######################################################################

package CondorGlue;

use DBI;
use File::Basename;

# database parameters
my $database = "history";
my $username = "nmi";
my $password = "nmi-nwo-nmi";
my $DB_CONNECT_STR = "DBI:mysql:database=$database;host=localhost";
my $RUN_TABLE = "Run";

# Location of the nightly build tag file
my $tag_file_url = "http://www.cs.wisc.edu/condor/nwo-build-tags";

sub printPrereqs
{
    my $fh = shift;

    # global prereqs
    print $fh "prereqs = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, binutils-2.15\n";

    # platform-specific prereqs
    print $fh "prereqs_sun4u_sol_5.9 = gcc-2.95.3, coreutils-5.2.1\n";
    print $fh "prereqs_sun4u_sol_5.8 = gcc-2.95.3, coreutils-5.2.1\n";
    print $fh "prereqs_ppc_aix_5.2 = vac-6, vacpp-6, coreutils-5.2.1\n";
    print $fh "prereqs_x86_rh_9 = coreutils-5.2.1\n";
    print $fh "prereqs_x86_rh_8.0 = coreutils-5.2.1\n";
    print $fh "prereqs_x86_rh_7.2 = coreutils-5.2.1\n";

    # it's not clear that we need coreutils at all, but until we have
    # a chance to experiment and be sure, we'll keep including them.
    # it's a dangerous prereq, since it's got 89 programs in it,
    # including a busted version of uname that confused the gpt build
    # on the osx box...

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
    run( "wget --tries=1 --non-verbose $tag_file_url" );

    my %tags;

    open(TAGS, $tag_file) || 
	die "Can't read nightly tag file $tag_file: $!\n";
    while (<TAGS>) {
        chomp($_);
        my @tag = split /\s/, $_;
        $tags{$tag[1]} = $tag[0]; 
    }
    close(TAGS);

    return %tags;
}

sub printBuildUsage
{
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


sub makeFetchFile
{
    my ( $file, $tag, $module ) = @_;

    open( FILE, ">$srcsfile" ) || die "Can't open $file for writing: $!\n";
    print FILE "method = cvs\n";
    print FILE "cvs_root = :ext:cndr-cvs\@chopin.cs.wisc.edu:/p/condor/repository/CONDOR_SRC\n";
    print FILE "cvs_server = /afs/cs.wisc.edu/p/condor/public/bin/auth-cvs\n";
    print FILE "cvs_rsh = /nmi/scripts/ssh_no_x11\n";
    print FILE "cvs_tag = $tag\n";
    print FILE "cvs_module = $module\n";
    close FILE;
}


1;

