#!/usr/bin/env perl
use warnings;
use strict;

# This script accepts a user workspace containing Condor sources and externals,
# and initiates an NMI build job.

# Currently [5/05] works on grandcentral only but will be extended for more
# general cases.

use Getopt::Long;
use Cwd;
use DBI;
use File::Spec;

use vars qw/$opt_workspace $opt_platforms $opt_notify $opt_help/;

# Database parameters.
my $database = "history";
my $username = "nmi";
my $password = "nmi-nwo-nmi";
my $DB_CONNECT_STR = "DBI:mysql:database=$database;host=localhost";
my $RUN_TABLE = "Run";

GetOptions('workspace=s' => $opt_workspace,
	   'platforms=s' => $opt_platforms,
	   'notify=s' => $opt_notify,
	   'help' => $opt_help);

if (defined $opt_help) {
  print "Usage: $0 [--help] [--workspace=<path>] [--notify=<address>] [--platforms=<list>]\n";
  exit 0;
}

unless (defined $opt_workspace) {
  print "You must specify a workspace. Exiting...\n";
  exit 1;
}

my @all_platforms = qq(x86_rh_9 x86_rh_8.0 x86_rh_7.2 ia64_sles_8 sun4u_sol_5.9 sun4u_sol_5.8
ppc_aix_5.2 x86_winnt_5.1 hppa_hpux_B.10.20 ppc_macosx);
my $platforms = (defined $opt_platforms)? $opt_platforms : join '.', @all_platforms;
my $whoami = `whoami`;
chomp $whoami;
my $notify = (defined $opt_notify)? $opt_notify : $whoami . '@cs.wisc.edu';

my $init_cwd = getcwd;
my $user_ws = File::Spec->rel2abs($opt_workspace);    # removes trailing slash as needed, sweet!
die "Invalid directory: $user_ws" unless -d $user_ws;

my $type = 'build';
$ENV{PATH} = "/nmi/bin:/usr/local/condor/bin:/usr/local/condor/sbin:"
  . $ENV{PATH};

my $workspace = "/tmp/condor_$type." . "$$";
mkdir $workspace or die "Can't create workspace $workspace: $!\n";
chdir $workspace or die "Can't chdir($workspace): $!\n";

# Prepare directories to be copied.
my @inputs;
opendir D, $user_ws or die "Could not opendir $user_ws: $!";
while (defined (my $dir = readdir D)) {
  next if $dir eq '.' or $dir eq '..';
  next unless -d "$user_ws/$dir";
  my $input_file = "$dir.scp";
  open F, ">$input_file" or die "Could not open $input_file: $!";
  print F <<"END";
method = scp
scp_file = $user_ws/$dir
recursive = true
END
  close F or die "Could not close $input_file: $!";
  push @inputs, $input_file;
}

# Generate the cmdfile.
my $cmdfile = 'condor_cmdfile';
my $inputs = join ',', @inputs;
open CMD, ">$cmdfile" or die "Can't open $cmdfile for writing: $!\n";
print CMD <<"END";
project = condor
component = condor
notify = $notify
private_web_users = condor-team
priority = 1
inputs = $inputs
run_type = build
platforms = $platforms
pre_all = nmi_glue/build/pre_all
remote_declare = nmi_glue/build/remote_declare
remote_pre = nmi_glue/build/remote_pre
remote_task = nmi_glue/build/remote_task
remote_post = nmi_glue/build/remote_post
platform_post = nmi_glue/build/platform_post
prereqs_x86_rh_7.2 = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, binutils-2.15
prereqs_x86_rh_8.0 = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, binutils-2.15
prereqs_x86_rh_9 = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, binutils-2.15
prereqs_sun4u_sol_5.9 = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, gcc-2.95.3, binutils-2.15
prereqs_sun4u_sol_5.8 = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, gcc-2.95.3, binutils-2.15
prereqs_ppc_aix_5.2 = perl-5.8.5, tar-1.14, patch-2.5.4, m4-1.4.1, flex-2.5.4a, make-3.80, byacc-1.9, bison-1.25, gzip-1.2.4, vac-6, vacpp-6
prereqs_hppa_hpux_B.10.2 = everything-1.0.0
END
close CMD;

print "Submitting condor build.\n";
my $output_str=`/nmi/bin/nmi_submit $cmdfile`;
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

  my $BUILD_INFO_FILE = "$nmirundir/userdir/common/test_ids";
  print "BUILD_INFO_FILE = $BUILD_INFO_FILE\n";
  if ($runid) {
    open INFO_FILE, ">>$BUILD_INFO_FILE" or
      die "Unable to open $BUILD_INFO_FILE for writing: $!\n";
    print INFO_FILE "$runid\n";
    close INFO_FILE;
  }
} else {
  print "nmi_submit failed\n";
}

chdir( $init_cwd );
run( "rm -rf $workspace", 0 );
exit 0;

sub getRunid {
  my ($gid) = @_;
  my $runid = "";

  my $db = DBI->connect("$DB_CONNECT_STR","$username","$password") or
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
