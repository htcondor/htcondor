#! /usr/bin/env perl
use File::Copy;
use File::Path;
use Getopt::Long;
use Cwd;

my $home = getcwd();
my $storksubmitfile = "stork.sub";

GetOptions (
                'help' => \$help,
                'log' => \$log,
                'root=s' => \$root,
				'dag=s' => \$dag,
				'dagnm=s' => \$dagnm,
				'src_url=s' => \$src_url,
				'dest_url=s' => \$dest_url,
);

if ( $help )    { help() and exit(0); }

#$log = "Just Do IT!\n";
if ( $log )    
{
	my $LogFile = "sdmkdag.log";
	print "Build log will be in ---$LogFile---\n";
	open(OLDOUT, ">&STDOUT");
	open(OLDERR, ">&STDERR");
	open(STDOUT, ">$LogFile") or die "Could not open $LogFile: $!\n";
	open(STDERR, ">&STDOUT");
	select(STDERR); $| = 1;
	select(STDOUT); $| = 1;
}

my $dagmanlog = "";

if(!$root) { help(); die "Must call this with --root=somedir!\n"; }
if(!$dag) { help(); die "Must call this with --dag=somedir!\n"; }
if(!$dagnm) { help(); die "Must call this with --dagnm=somename!\n"; }
if(!$src_url) { help(); die "Must call this with --src_url=somename!\n"; }
if(!$dest_url) { help(); die "Must call this with --dest_url=somename!\n"; }

$cmd = "$dagnm";
$testname = 'Condor submit dag - stork transfer test';
$dagman_args = "-v -storklog `condor_config_val LOG`/Stork.user_log";

if(! -d $root)
{
	system("mkdir -p  $root");
}

chdir("$root");
my $rootdir = getcwd();
print "Dag root directory is $rootdir\n";

if(! -d $dag)
{
	system("mkdir -p  $dag");
}

chdir("$dag");
my $dagdir = getcwd();
print "Dag root directory is $dagdir\n";

open(DAG,">$dagnm" ) || die "Can not create dag file<<$dagnm>>: $!\n";
print DAG "DATA	JOB1 $storksubmitfile\n";
close(DAG);

open(DAG,">$storksubmitfile") || die "Can not create dag submit file<<$storksubmitfile>>: $!\n";
print DAG "[\n";
print DAG "		dap_type = transfer\n";
print DAG "		src_url = $src_url\n";
print DAG "		dest_url = $dest_url\n";
print DAG "]\n";
close(DAG);

$executed = sub
{
        print "Good. We need the dag to run\n";
};

$submitted = sub
{
        print "submitted: This test will see submit, executing and successful completion\n";
};

$success = sub
{
        print "executed successfully\n";
};

CondorTest::RegisterExitedSuccess( $testname, $success);
CondorTest::RegisterExecute($testname, $executed);
CondorTest::RegisterSubmit( $testname, $submitted );

if( CondorTest::RunDagTest($testname, $cmd, 0, $dagman_args) ) {
        print "$testname: SUCCESS\n";
        exit(0);
} else {
        die "$testname: CondorTest::RunTest() failed\n";
}


exit();

sub help
{
print "Usage: sdmkdag.pl --root=rootdir --dag=dagdir --dagnm=dagname --src_url=file --dest_url=file
	Options:
		[-h/--help]                  See this
		[-l/--log]                   Create log
		[-r/--root]                  Directory name to create DAG directory
		[--dag]                      Name of DAG directory
		[--dagnm]                    Name of DAG
		[--src_url]                  Name of source
		[--dest_url]                 Name of destination
		\n";
}
