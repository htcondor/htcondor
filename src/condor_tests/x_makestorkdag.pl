#! /usr/bin/env perl
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
$testdesc =  'Condor submit dag - stork transfer test';
$testname = "x_makestorkdag";
#$dagman_args = "-v -storklog `condor_config_val LOG`/Stork.user_log";
$dagman_args = "-v ";

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
print DAG "		dap_type = transfer;\n";
print DAG "		src_url = \"file:$src_url\";\n";
print DAG "		dest_url = \"file:$dest_url\";\n";
print DAG "		log = \"$dagdir/stork.log\";\n";
print DAG "]\n";
close(DAG);

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
