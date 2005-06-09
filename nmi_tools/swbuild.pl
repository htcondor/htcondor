#!/usr/bin/env perl

# This script initiates an NMI build of a given software product from
# a tar.gz tarball. It is assumed that the build procedure is:
# gunzip + untar + configure + make. Will be generalized as needed but
# I don't want to overengineer the script too early.   --Tolya

use warnings;
use strict;
use Getopt::Long;
use File::Spec;
use Cwd;
use DBI;
use File::Basename;

$ENV{PATH} = "/nmi/bin:" . $ENV{PATH};
my $init_cwd = getcwd;

# When the remote program wakes up it will see the tarball and any glue
# files we supply.

my $usage = "usage: $0 <tar_gz_tarball> [--platforms=<platforms>] [--notify=<address>] [--help]\n";
GetOptions('help' => $main::opt_help,
	   'platforms=s' => $main::opt_platforms,
	   'notify=s' => $main::opt_notify);
if (defined $main::opt_help) {
  print $usage;
  exit 0;
}

die $usage unless @ARGV == 1;

my $tarball_path = File::Spec->rel2abs($ARGV[0]);

-f $tarball_path or die "No such file: $tarball_path";
-r $tarball_path or die "File $tarball_path is not readable.";

my $tarball = basename($tarball_path);

die "$tarball must really be a tar.gz file!" unless $tarball =~ m/^(.+)\.tar\.gz$/;
my $swname = $1;

# Database parameters.
my $database = "history";
my $username = "nmi";
my $password = "nmi-nwo-nmi";
my $DB_CONNECT_STR = "DBI:mysql:database=$database;host=localhost";
my $RUN_TABLE = "Run";

# Note: windows not included.
my @all_platforms = qw(x86_rh_9 x86_rh_8.0 x86_rh_7.2 ia64_sles_8 sun4u_sol_5.9 sun4u_sol_5.8
		       ppc_aix_5.2 hppa_hpux_B.10.20 ppc_macosx);
my $platforms = (defined $main::opt_platforms)? $main::opt_platforms : join ',', @all_platforms;
my $whoami = `whoami`;
chomp $whoami;
my $notify = (defined $main::opt_notify)? $main::opt_notify : "$whoami\@cs.wisc.edu";

my $workspace = "/tmp/${swname}_build" . "$$";
mkdir $workspace or die "Can't create workspace $workspace: $!\n";
chdir $workspace or die "Can't chdir($workspace): $!\n";

# Create an input file for the source tarball.
my $src_input_file = "${swname}-src.scp";
my $str =  <<"END";
method = scp
scp_file = $tarball_path
END
make_file($src_input_file, $str);

### Create glue scripts.
my $glue_rel_path = 'glue';
mkdir $glue_rel_path or die "Can't create directory '$glue_rel_path' : $!\n";
my $glue_path = File::Spec->rel2abs($glue_rel_path);

# Create remote_pre glue script.
$str = <<"END";
#!/bin/sh
echo PATH=\$PATH
cat $tarball | gunzip | tar xvf -
END
my $remote_pre = "$glue_rel_path/untar.sh";
make_file($remote_pre, $str, '0755');

# Create remote_declare glue script.
$str = <<'END';
#!/bin/sh
for name in $*; do
    echo $name >> tasklist.nmi
done
END
my $remote_declare = "$glue_rel_path/declare_steps.sh";
make_file($remote_declare, $str, '0755');

# Create remote_task glue script.
$str = <<"END";
#!/bin/sh
TASK=\$_NMI_TASKNAME
cd $swname || exit \$?
if [ \$TASK = "configure" ]; then
    ./configure
else
    make \$TASK
fi
END
my $remote_task = "$glue_rel_path/do_build.sh";
make_file($remote_task, $str, '0755');

# Create remote_post glue script.
$str = <<"END";
#!/bin/sh
tar cf results.tar $swname
gzip -9 results.tar
END
my $remote_post = "$glue_rel_path/collect_results.sh";
make_file($remote_post, $str, '0755');

# Now create an input file for all that glue.
my $glue_input_file = "${swname}-glue.scp";
$str = <<"END";
method = scp
scp_file = $glue_path
recursive = true
END
make_file($glue_input_file, $str);

# Create an NMI submit file. NOTE: for now hard-coded the stupid per-platform
# prereq requirements but the impression is a bit ugly.
$str = << "END";
description = $swname build
project = $swname
project_release = x,x,x
component = $swname
component_version = x,x,x
priority = 1
module = $swname

notify = $notify
inputs = $src_input_file, $glue_input_file
remote_pre = $remote_pre
remote_declare = $remote_declare
remote_declare_args = configure all test
remote_task = $remote_task
remote_post = $remote_post

platforms = $platforms

prereqs_sun4u_sol_5.9 = perl-5.8.5, tar-1.14, make-3.80, gcc-2.95.3
prereqs_sun4u_sol_5.8 = perl-5.8.5, tar-1.14, make-3.80, gcc-2.95.3
prereqs_ppc_aix_5.2 = perl-5.8.5, tar-1.14, make-3.80, vac-6, vacpp-6
prereqs_alpha_osf_V5.1 = perl-5.8.5, tar-1.14, make-3.80, gcc-2.95.3
prereqs_irix_6.5 = perl-5.8.5, tar-1.14, make-3.80, usr_freeware-1.0
prereqs_hppa_hpux_B.10.20 = everything-1.0.0

run_type = build
END
my $nmi_submit = "${swname}.cmd";
make_file($nmi_submit, $str);

print "Submitting $swname build...\n";
my $output_str=`/nmi/bin/nmi_submit $nmi_submit`;
my $status = $?;
if( not $status ) {
  # Sleep for some time till the records are available in db.
  sleep 30;
  my @lines = split("\n", $output_str);
  my $nmirundir = $lines[15];

  my @toks = split '/', $nmirundir;
  my $gid = $toks[3];

  my $runid = getRunid($gid);
  print "I have guessed that the runid = $runid should be tested\n";

  } else {
  print "nmi_submit failed\n";
}

chdir( $init_cwd );
run( "rm -rf $workspace", 0 );
exit 0;

sub getRunid {
  my ($gid) = @_;
  my $runid = "";

  my $db = DBI->connect($DB_CONNECT_STR, $username, $password) or
    die "Could not connect to database: $DBI::errstr\n";
  my $cmd_str = qq/SELECT runid from $RUN_TABLE WHERE gid='$gid'/;
  print "$cmd_str\n";

  my $handle = $db->prepare("$cmd_str") or die "Cannot prepare SQL statement: $DBI::errstr";
  $handle->execute() or die "Cannot execute SQL statement: $DBI::errstr";
  while ( my $row = $handle->fetchrow_hashref() ) {
    $runid = $row->{'runid'};
    last;
  }
  $handle->finish();
  $db->disconnect or warn "Error disconnecting: $DBI::errstr";

  return $runid;
}

sub run
{
  my ($cmd, $fatal) = @_;
  my $ret;
  my $output = '';

  # if not specified, the command is fatal.
  if (!defined($fatal) or ($fatal != 0 and $fatal != 1)) {
    $fatal = 1;
  }

  print "\n";
  print "#  $cmd\n";

  # Run the command.
  system("($cmd)  </dev/null 2>&1");
  $ret = $? / 256;

  # Should we die here?
  if ($fatal && $ret != 0) {
    print "\n";
    print "FAILED COMMAND: $cmd\n";
    print "RETURN VALUE:  $ret\n";
    print "\n";
    exit(1);
  }

  # Return the command's return value.
  return $ret;
}

# Quick sub to populate file with a (not too long) string.
sub make_file {
  my ($file, $str, $mode) = @_;
  open F, ">$file" or die "Could not open file $file for writing: $!";
  print F $str;
  close F or die "Could not close file $file: $!";
  return unless $mode;
  chmod oct($mode), $file or die "Could not chmod $mode $file: $!";
}

