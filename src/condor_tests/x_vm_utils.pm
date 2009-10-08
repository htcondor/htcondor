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

package x_vm_utils;

use strict;
use Cwd;

use CondorTest;
use CondorPersonal;

my $DATA_SIZE = 1440 * 1024;
my $locconfig = "";

sub setup_data_disk
{
	my $corename = shift;

	# read in input script, pad it out with newlines to be 1 MB,
	# then write it out as data disk for VM to use
	#
	open(CODE_SOURCE, "<../$corename.sh") ||
		die "error opening source ../$corename.sh for data disk: $!\n";
	my $code_source;
	my $bytes = read(CODE_SOURCE, $code_source, $DATA_SIZE);
	if (not defined $bytes) {
	    die "error reading source file for data disk: $!\n";
	}
	close(CODE_SOURCE);
	set_data($code_source);

	# add the absolute path for our data disk to
	# the VMX file via a VMWARE_LOCAL_SETTINGS_FILE
	#
	open(VLS_FILE, ">vmware_local_settings_file") ||
		die "error opening VMX file: $!\n";
	my $cwd = Cwd::cwd();
	print VLS_FILE "floppy0.startConnected = \"TRUE\"\n";
	print VLS_FILE "floppy0.fileType = \"file\"\n";
	print VLS_FILE "floppy0.fileName = \"$cwd/test_data.img\"\n";
	close(VLS_FILE);
}

sub initialize
{
	my $corename = shift;

	my $tarball = "x_vmware_test_vm.tar.gz";
	if ( ! -f $tarball ) {
		my $fetch_prog;
		my $fetch_url = "http://parrot.cs.wisc.edu/externals/$tarball";
		if ( $^O eq "darwin" ) {
			$fetch_prog = "curl -O";
		} else {
			# Retry up to 60 times, waiting 22 seconds between tries
			$fetch_prog = "wget --tries=60 --timeout=22";
		}
		CondorTest::debug("Fetching $fetch_url\n",1);
		my $rc = system( "$fetch_prog $fetch_url 2>&1" );
		if ($rc) {
			my $status = $rc >> 8;
			CondorTest::debug("URL retrieval failed: $status\n",1);
			die "VM image not fetched\n";
		}
	}

	# FIXME: for now, VMware is not a real prereq, so we are
	# stuck with this messiness
	#
	my $oldpath = $ENV{PATH};
	my $newpath = "/prereq/VMware-server-1.0.7/bin:" . $oldpath;
	$ENV{PATH} = $newpath;

	unless ( system("vmrun list") == 0 && system("vmware-cmd -l") == 0 &&
			 system("mkisofs -version") == 0 ) {
		CondorTest::debug("VMWare tools exited with non-zero status\n",1);
		die "VMWare tools failed\n";
	}

	# do the saveme thing so we know the name of the
	# directory that will house our personal Condor
	#
	unless (CondorPersonal::SaveMeSetup($corename)) {
		die "CondorPersonal::SaveMeSetup failed\n";
	}

	# copy the submit file template down into the saveme
	# dir
	#
	system("cp x_vmware_test_vm.cmd $corename.saveme");

	# we'll be setting up a VMWARE_LOCAL_SETTINGS_FILE when
	# we initialize our data disk. but here, we need to point
	# the personal Condor we are about to start at it
	#
	my $cwd = Cwd::cwd();
	$ENV{"_CONDOR_VMWARE_LOCAL_SETTINGS_FILE"} =
		"$cwd/$corename.saveme/vmware_local_settings_file";

	# start a personal Condor for the test
	#
	my $configloc = CondorTest::StartPersonal($corename,
	                                            "x_param.vmware",
	                                            "vmuniverse");
	print $configloc . "\n";
	my @local = split /\+/, $configloc;
	$locconfig = shift @local;
	$ENV{CONDOR_CONFIG} = $locconfig;

	# now chdir into the personal Condor directory so files produced
	# by the test will appear in there
	#
	chdir("$corename.saveme") ||
		die("failure to chdir into $corename.saveme\n");

	# untar the VM test image
	#
	my $vmimagetar = "../$tarball";
	if(!(-f "$vmimagetar")) {
		die "$vmimagetar not found for VM job\n";
	} else {
		system("tar -zxvf $vmimagetar");
	}

	# set up the data disk for this test
	#
	setup_data_disk($corename);
}

sub add_submit_command
{
	my $line = shift;
	open(CMD_FILE, ">>x_vmware_test_vm.cmd") ||
		die "error opening submit file: $!\n";
	print CMD_FILE "$line\n";
	close(CMD_FILE);
}

sub run_test
{
	my $testname = shift;
	add_submit_command("queue");
	return CondorTest::RunTest($testname, "x_vmware_test_vm.cmd", 0);
}

sub get_data
{
	open(DATA_DISK, "<test_data.img") ||
	    die "error opening file for data disk: $!\n";
	my $data = <DATA_DISK>;
	chomp($data);
	close(DATA_DISK);
	return $data;
}

sub set_data
{
	my $data = shift;
	$data .= "\n" x ($DATA_SIZE - length($data));
	open(DATA_DISK, ">test_data.img") ||
	    die "error opening file for data disk: $!\n";
	print DATA_DISK $data ||
	    die "error writing out data disk\n";
	close(DATA_DISK);
}

sub check_output
{
	my $output_check = shift;
	my $output = get_data();
	my $ret = $output_check eq $output;
	unless ($ret) {
		CondorTest::debug("expected: $output_check\n",1);
		CondorTest::debug("actual: $output\n",1);
	}
	return $ret;
}

sub cleanup
{
	# shut down the personal Condor
	#
	CondorTest::KillPersonal($locconfig);

	# delete the tarball contents
	#
	system("rm -rf x_vmware_test_vm");
}

1;
