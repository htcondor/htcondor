#!/usr/bin/perl

use Cwd;
use strict;
use Config;

# find the GPT directory

my($gpath, $gptpath);

$gptpath = $ENV{GPT_LOCATION};
$gpath = $ENV{GLOBUS_LOCATION};

#
# And the old standby..
#

if (!defined($gpath))
{
    die "GLOBUS_LOCATION needs to be set before running this script"
}

if (defined($gptpath))
{
    @INC = ("$gptpath/lib/perl", "$gpath/lib/perl", @INC);
}
else
{
    @INC = ("$gpath/lib/perl", @INC);
}

require Grid::GPT::Setup;

chdir($gpath);

# Create rumtime directory

`mkdir -p $gpath/var/condor`;
`chmod o+x $gpath/var/condor`;
`chmod g+x $gpath/var/condor`;

`mkdir -p $gpath/var/condor/log`;
`chmod o+x $gpath/var/condor/log`;
`chmod g+x $gpath/var/condor/log`;

`mkdir -p $gpath/var/condor/log/GridLogs`;
`chmod 1777 $gpath/var/condor/log/GridLogs/`;

`mkdir -p $gpath/var/condor/spool`;
`chmod 1777 $gpath/var/condor/spool`;

my $globusdir = $ENV{GLOBUS_LOCATION};

`cp $globusdir/setup/globus/condor/SXXcondor.in $globusdir/sbin/SXXcondor`;
`chmod 0755 $globusdir/sbin/SXXcondor`;
my $metadata = new Grid::GPT::Setup(package_name => "condor_base_setup");

$metadata->finish();


sub fixpaths
{
    my $g;
	my $h;


    #
    # Set up path translations for the installation files
    #

    my %def = (
        "\@GLOBUS_LOCATION\@" => "${globusdir}",
        );

    #
    # Files on which to perform path translations
    #

    my @files = (
        "$globusdir/sbin/SXXcondor",
        );

    for my $f (@files)
    {
        $f =~ /(.*\/)*(.*)$/;

        #
        # we really should create a random filename and make sure that it
        # doesn't already exist (based off current time_t or something)
        #

        $g = "$f.tmp";
        #
        # What is $f's filename? (taken from the qualified path)
        #

        $h = $f;
        $h =~ s#^.*/##;

        #
        # Grab the current mode/uid/gid for use later
        #

        my $mode = (stat($f))[2];
        my $uid = (stat($f))[4];
        my $gid = (stat($f))[5];

        #
        # Move $f into a .tmp file for the translation step
        #
        my $result = system("mv $f $g 2>&1");
        if ($result or $?)
        {
            die "ERROR: Unable to execute command: $!\n";
        }

        open(IN, "<$g") || die ("$0: input file $g missing!\n");
        open(OUT, ">$f") || die ("$0: unable to open output file $f!\n");

        while (<IN>)
        {
            for my $s (keys(%def))
            {
                s#$s#$def{$s}#;
            } # for $s
            print OUT "$_";
        } # while <IN>

        close(OUT);
        close(IN);

        #
        # Remove the old .tmp file
        #

        $result = system("rm $g 2>&1");

        if ($result or $?)
        {
            die "ERROR: Unable to execute command: $!\n";
        }

        #
        # An attempt to revert the new file back to the original file's
        # mode/uid/gid
        #

        chmod($mode, $f);
        chown($uid, $gid, $f);

        print "$h\n";
    } # for $f

    return 0;
}

fixpaths();
