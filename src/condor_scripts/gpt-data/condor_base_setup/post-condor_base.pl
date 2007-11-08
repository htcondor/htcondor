#!/usr/bin/perl
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************


use Cwd;
use strict;
use Config;

my $condor_uid;
my $condor_id;
my $condor_home;
my @pwdent;

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

###  Figure out who should own the Condor directories we create.
$condor_uid = &find_owner();

# Create runtime directory

`mkdir -p $gpath/var/condor`;
`chown $condor_uid $gpath/var/condor`; 
`chmod 0777 $gpath/var/condor`;

`mkdir -p $gpath/var/condor/log`;
`chown $condor_uid $gpath/var/condor/log`;
`chmod 0777 $gpath/var/condor/log`;

`mkdir -p $gpath/var/condor/log/GridLogs`;
`chown $condor_uid $gpath/var/condor/log/GridLogs/`;
`chmod 1777 $gpath/var/condor/log/GridLogs/`;

`mkdir -p $gpath/var/condor/spool`;
`chown $condor_uid $gpath/var/condor/spool`;
`chmod 0777 $gpath/var/condor/spool`;

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

sub find_owner {
	my $ownerret;
    if( $< ) {
	# Non-root
	return $<;
    }
    # We're root, see who should own the Condor files/directories
    $_=$ENV{CONDOR_IDS};
    if( $_ ) {
	s/(\d*)\.\d*/$1/;
	$ownerret = $_;
	@pwdent = getpwuid($ownerret);
	if( ! $pwdent[0] ) {
	    print "\nuid specified in CONDOR_IDS ($ownerret) is not valid.  Please ",
	    "set the\nCONDOR_IDS environment variable to the uid.gid pair ",
	    "that Condor\nshould use.\n\n";
	    exit(1);
	}
    } 

    ###  Find condor's home directory, if it exists.
    @pwdent = getpwnam( "condor" );
    $condor_home=$pwdent[7];
    $condor_uid=$pwdent[2];

    if( $condor_uid ) {
	$ownerret = $condor_uid;
    } elsif (@pwdent = getpwnam ("daemon")) {
	$ownerret=$pwdent[2];
	print "Condor is being configured to use the daemon user, which "
			. "will work, but usually isn't "
			. "what you want. Usually it's the condor user, which doesn't "
			. "exist on this system. \n";
    } else{
	print "There's no \"condor\" account on this machine and the CONDOR_IDS ",
	"environment\nvariable is not set.  Please create a condor account, or ",
	"set the CONDOR_IDS\nenvironment variable to the uid.gid pair that ",
	"Condor should use.\n";
	exit(1);
    }
    return $ownerret;
}

