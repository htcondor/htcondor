#!/usr/bin/env perl
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

######################################################################
###### WARNING!!!  The return value of this script has special  ######
###### meaning, so you can NOT just call die().  you MUST       ######
###### use the special c_die() method so we return 3!!!!        ######
######################################################################
######################################################################
# Perform a given build task, and return the status of that task
# 0 = success
# 1 = build failed
# 3 = internal fatal error (a.k.a. die)
######################################################################

use strict;
use warnings;
use Cwd;
use File::Basename;
my $BaseDir = getcwd();

my $EXTERNALS_TASK        = "remote_task-externals";
my $BUILD_TASK            = "remote_task-build";
my $TAR_TASK              = "remote_task-create_tar";
my $TAR_TESTS_TASK        = "remote_task-create_tests_tar";
my $CHECK_TAR_TASK        = "remote_task-check_tar";
my $UNSTRIPPED_TASK       = "remote_task-create_unstripped_tar";
my $CHECK_UNSTRIPPED_TASK = "remote_task-check_unstripped_tar";
my $NATIVE_DEBUG_TASK     = "remote_task-create_native_unstripped";
my $NATIVE_TASK           = "remote_task-create_native";
my $CHECK_NATIVE_TASK     = "remote_task-check_native";
my $BUILD_TESTS_TASK      = "remote_task-build_tests";
my $RUN_UNIT_TESTS        = "remote_task-run_unit_tests";
my $COVERITY_ANALYSIS     = "remote_task-coverity_analysis";
my $EXTRACT_TARBALLS_TASK = "remote_task-extract_tarballs";

my $taskname = "";
my $execstr = "";
my $VCVER = "VC11";

# autoflush our STDOUT
$| = 1;

if( defined $ENV{_NMI_STEP_FAILED} ) { 
    my $exit_status = 1;
    my $nmi_task_failure = "$ENV{_NMI_STEP_FAILED}";
    print "A previous step failed.  _NMI_STEP_FAILED is: '$ENV{_NMI_STEP_FAILED}'\n";
    print "Exiting now with exit status $exit_status.\n";
    exit $exit_status;
}
else {
    print "The _NMI_STEP_FAILED variable is not set\n";
}


## TSTCLAIR->THAWAN et.al ok below is the conundrum as it relates to packaging
## The remote declare needs to be created/updated for (N) different packages
## b/c of the issues with multiple PATHS on install targets.  As a result
## cmake will need to be called again with the same args, which you may want
## to store in a file during the remote pre phase.

if( ! defined $ENV{_NMI_TASKNAME} ) {
    print "NMI_TASKNAME not defined, building tar.gz package as default";
    $taskname = $TAR_TASK;
}
else {
    $taskname = $ENV{_NMI_TASKNAME};
}

chomp(my $hostname = `hostname -f`);
print "Executing task '$taskname' on host '$hostname'\n";

# Build with warnings == errors on Fedora
my $werror="";
if ($ENV{NMI_PLATFORM} =~ /_fedora(_)?[123][0-9]/i) {
    $werror = "-DCONDOR_C_FLAGS:STRING=-Werror";
}

# enable use of VisualStudio 9 2008 vs9 on the Windows7 platform
my $enable_vs14 = 0;
my $enable_vs9 = 0;
#uncomment to use vs9 on Win7 platform# if ($ENV{NMI_PLATFORM} =~ /Windows7/i) { $enable_vs9 = 1; }
if ($enable_vs9 && $ENV{VS90COMNTOOLS} =~ /common7/i) {
	$VCVER = "VC9";
}
if ($ENV{NMI_PLATFORM} =~ /Windows10/i) { $enable_vs14 = 1; }
if ($enable_vs14 && $ENV{VS140COMNTOOLS} =~ /common7/i) {
	$VCVER = "VC14";
}



# Checking task type
if( $taskname eq $EXTERNALS_TASK ) {
    # Since we do not declare the externals task on Windows, we don't have
    # to handle invoking the Windows build tools in this step.
    my $num_cores = 1;
    if (defined($ENV{"OMP_NUM_THREADS"})) {
	$num_cores = $ENV{"OMP_NUM_THREADS"};
    }
    $execstr = "make VERBOSE=1 -j ${num_cores} externals";
}
elsif( $taskname eq $BUILD_TASK ) {
    # Since we do not declare the externals task on Windows, we don't have
    # to handle invoking the Windows build tools in this step.
    my $num_cores = 1;
    if (defined($ENV{"OMP_NUM_THREADS"})) {
	$num_cores = $ENV{"OMP_NUM_THREADS"};
    }
    $execstr = get_cmake_args();
    $execstr = $execstr . " ${werror} -DCONDOR_PACKAGE_BUILD:BOOL=OFF -DCONDOR_STRIP_PACKAGES:BOOL=OFF && make VERBOSE=1 -j ${num_cores} install";
}
elsif( $taskname eq $BUILD_TESTS_TASK ) {
    $execstr = "make VERBOSE=1 tests";
}
elsif ($taskname eq $UNSTRIPPED_TASK) {
    #$execstr = get_cmake_args();
    #Append extra argument
    #$execstr = $execstr . " ${werror} -DCONDOR_PACKAGE_BUILD:BOOL=OFF -DCONDOR_STRIP_PACKAGES:BOOL=OFF && make VERBOSE=1 targz";
    $execstr = "make VERBOSE=1 targz";
}
elsif ($taskname eq $CHECK_UNSTRIPPED_TASK) {
    my $tarball_check_script = get_tarball_check_script();
    my $tarball = get_tarball_name();
    $execstr = "$tarball_check_script $tarball";
}
elsif ($taskname eq $TAR_TASK) { 
    #Normal build -> create tar.gz package (The only reason install is done is for the std:u:tests) 
    #Reconfigure cmake variables for stripped tarball build
    $execstr = get_cmake_args();
    $execstr = $execstr . " ${werror} -DCONDOR_PACKAGE_BUILD:BOOL=OFF -DCONDOR_STRIP_PACKAGES:BOOL=ON && make VERBOSE=1 install/strip tests && make VERBOSE=1 targz";
}
elsif ($taskname eq $TAR_TESTS_TASK) {
    $execstr = "make VERBOSE=1 tests-tar-pkg";
}
elsif ($taskname eq $CHECK_TAR_TASK) {
    my $tarball_check_script = get_tarball_check_script();
    my $tarball = get_tarball_name();
    $execstr = "$tarball_check_script $tarball";
}
elsif ($taskname eq $NATIVE_TASK || $taskname eq $NATIVE_DEBUG_TASK) {
    my $is_debug = ($taskname eq $NATIVE_DEBUG_TASK);
    # Create native packages on Red Hat and Debian
    print "Create native package.  NMI_PLATFORM = $ENV{NMI_PLATFORM}\n";
    if ($ENV{NMI_PLATFORM} =~ /(deb|ubuntu)/i) {
        print "Detected OS is Debian or Ubuntu.  Creating Deb package.\n";
        $execstr = create_deb($is_debug);
    }
    elsif ($ENV{NMI_PLATFORM} =~ /(rha|redhat|fedora|almalinux|amazonlinux|centos|opensuse|rocky|sl)/i) {
        print "Detected OS is Red Hat.  Creating RPM package.\n";
        $execstr = create_rpm($is_debug);
    }
    elsif ($ENV{NMI_PLATFORM} =~ /_win/i) {
        $execstr= "nmi_tools\\glue\\build\\build.win.bat NATIVE $VCVER";
    }
    else {
        print "We do not generate a native package for this platform.\n";
        exit 0;
    }
}
elsif ($taskname eq $CHECK_NATIVE_TASK) {
    # Validate the native packages
    print "Validating native package.  NMI_PLATFORM = $ENV{NMI_PLATFORM}\n";
    if ($ENV{NMI_PLATFORM} =~ /(deb|ubuntu)/i) {
        print "Detected OS is Debian.  Validating Deb package.\n";
        $execstr = check_deb();
    }
    elsif ($ENV{NMI_PLATFORM} =~ /(rha|redhat|fedora|almalinux|amazonlinux|centos|opensuse|rocky|sl)/i) {
        print "Detected OS is Red Hat.  Validating RPM package.\n";
        $execstr = check_rpm();
    }
    else {
        print "We do not generate a native package for this platform.\n";
        exit 0;
    }
} elsif ($taskname eq $RUN_UNIT_TESTS) {
    if ($ENV{NMI_PLATFORM} =~ /_win/i) {
    } else {
        $execstr = "ctest -v --output-on-failure -L batlab";
    }
} elsif ($taskname eq $COVERITY_ANALYSIS) {
	print "Running Coverity analysis\n";
	$ENV{PATH} = "/bin:$ENV{PATH}:/usr/local/coverity/cov-analysis-linux64-2020.06/bin";
	#$execstr = get_cmake_args();
	$execstr = "/usr/bin/cmake3 ";
	$execstr .= " -DBUILD_TESTING:bool=false -DWITH_SCITOKENS:bool=false";
	$execstr .= " && cd src && make clean && mkdir -p ../public/cov-data && cov-build --dir ../public/cov-data make -k ; cov-analyze --dir ../public/cov-data && cov-commit-defects --dir ../public/cov-data --stream htcondor --host batlabsubmit0001.chtc.wisc.edu --user admin --password `cat /usr/local/coverity/.p`";
} elsif ($taskname eq $EXTRACT_TARBALLS_TASK) {
    $execstr = get_extract_tarballs_script();
    $execstr .= " .";
}



if ($ENV{NMI_PLATFORM} =~ /_win/i) {
    # change exestr for Windows tasks.
    # build.win.bat uses BASE_DIR to setup some things.
    $ENV{BASE_DIR} = "$BaseDir";

    # get the build id and bitness out of the cmake args, we have to
    # pass it to the ZIP and MSI building steps
    #
    my $cmake_args = get_cmake_args();
    print "   cmake_args = $cmake_args\n";
    my $buildid = "";
    my $win64 = "x86";
    if ($cmake_args =~ /-DBUILDID:STRING=([0-9]+)/) { $buildid = $1; }
    if ($cmake_args =~ /-G[ ]*"Visual Studio ([0-9 ]+)/i) {
        $VCVER = "VC$1";
        if ($VCVER =~ /^VC([0-9]+)\s+([0-9]+)\s*/) { $VCVER = "VC$1"; }
        if ($cmake_args =~ /\s+Win64/i) { $win64 = "x64"; }
        if ($1 >= 17) { $win64 = "x64"; } # starting with VC17 there is no x86 vs x64
        print "cmake was invoked with a Visual Studio $VCVER $win64 generator\n";
    }
    if ($taskname eq $EXTERNALS_TASK) {
        $execstr= "nmi_tools\\glue\\build\\build.win.bat EXTERNALS $VCVER $win64 $buildid";
    } elsif ($taskname eq $BUILD_TASK) { # formerly $UNSTRIPPED_TASK
        $execstr= "nmi_tools\\glue\\build\\build.win.bat BUILD $VCVER $win64 $buildid";
    } elsif ($taskname eq $BUILD_TESTS_TASK) {
        $execstr= "nmi_tools\\glue\\build\\build.win.bat BLD_TESTS $VCVER $win64 $buildid";
    } elsif ($taskname eq $TAR_TESTS_TASK) {
        print "Windows CREATE_TESTS_TAR task\n";
        $execstr= "nmi_tools\\glue\\build\\build.win.bat TAR_TESTS $VCVER $win64 $buildid";
    } elsif ($taskname eq $NATIVE_TASK) {
        print "Windows NATIVE (MSI) task\n";
        $execstr= "nmi_tools\\glue\\build\\build.win.bat NATIVE $VCVER $win64 $buildid";
    } elsif ($taskname eq $TAR_TASK) {
        print "Windows CREATE_TAR (ZIP) task\n";
        $execstr= "nmi_tools\\glue\\build\\build.win.bat ZIP $VCVER $win64 $buildid";
    }
}
elsif ($ENV{NMI_PLATFORM} =~ /macos/i) {
    # CRUFT Once 9.0 is EOL, remove the python.org version of python3
    #   from the mac build machines and remove this setting of PATH.
    # Ensure we're using the system python3
    $ENV{PATH} ="/usr/bin:$ENV{PATH}";
    $ENV{DYLD_LIBRARY_PATH} ="$BaseDir/release_dir/lib:$BaseDir/release_dir/lib/condor";
    $ENV{PYTHONPATH} ="$BaseDir/release_dir/lib/python";
} else {
    $ENV{LD_LIBRARY_PATH} ="$BaseDir/release_dir/lib:$BaseDir/release_dir/lib/condor";
    $ENV{PYTHONPATH} ="$BaseDir/release_dir/lib/python";
}

######################################################################
# build
######################################################################

# Redirecting our STDERR to STDOUT means that we get them interspersed in the
# build output.  Metronome usually splits STDOUT and STDERR into separate
# files and putting them together makes it easier to correlate messages between
# the two.  We are redirecting this after dumping the environment to STDERR
# since it is nice to have that available, but it is annoying to have it at
# the top of every step.
open STDERR,">&STDOUT";

print "------------------------- ENV DUMP ------------------------\n";
foreach my $key ( sort {uc($a) cmp uc($b)} (keys %ENV) ) {
    print "$key=".$ENV{$key}."\n";
}
print "------------------------- ENV DUMP ------------------------\n";

print "Executing the following command:\n$execstr\n";
my $buildstatus = system("$execstr");

# now check the build status and return appropriately
if( $buildstatus == 0 ) {
    print "Completed task $taskname: SUCCESS\n";
    exit 0;
}

print "Completed task $taskname: FAILURE (Exit status: '$buildstatus')\n";
exit 1;



sub get_cmake_args {
    #Read back configuration from remote_pre stage
    my $cmake_cmd;

    # A comment above says never to use die() and that we must always return '3' on error.  But the
    # original code used die() when I took it over.  So I'm not sure which to follow.  I'll just
    # leave the die() in here for now... -scot 2011/03
    open(CMAKECONFIG, '<', "cmake_args.txt") or die "Cannot open cmake_args.txt: $!";
    while(<CMAKECONFIG>) {
        $cmake_cmd = $_;
        last;
    }
    close (CMAKECONFIG);
    return $cmake_cmd;
}

sub get_tarball_check_script {
    return dirname($0) . "/check-tarball.pl";
}

sub get_extract_tarballs_script {
    if ($ENV{NMI_PLATFORM} =~ /(RedHat|AlmaLinux|AmazonLinux|CentOS|Fedora|openSUSE|Rocky|SL)/) {
        return dirname($0) . "/make-tarball-from-rpms";
    }
    if ($ENV{NMI_PLATFORM} =~ /(deb|ubuntu)/i) {
        return dirname($0) . "/make-tarball-from-debs";
    }
}

sub get_tarball_name {
    if($taskname eq $CHECK_UNSTRIPPED_TASK) {
	return <*-unstripped.tar.gz>;
    }
    elsif($taskname eq $CHECK_TAR_TASK) {
	return <*-stripped.tar.gz>;
    }
    print "ERROR: get_tarball_task() called from a task that does not have an associated tarball.\n";
    exit 3;
}

sub create_rpm {
    my $is_debug = $_[0];
    if ($ENV{NMI_PLATFORM} =~ /(RedHat|AlmaLinux|AmazonLinux|CentOS|Fedora|openSUSE|Rocky|SL)/) {
        # Use native packaging tool
        return dirname($0) . "/build_uw_rpm.sh";
    } else {
        # Reconfigure cmake variables for native package build   
        my $command = get_cmake_args();
        my $strip = "ON"; if ($is_debug) { $strip = "OFF"; }
        return "$command -DCONDOR_PACKAGE_BUILD:BOOL=ON -DCONDOR_STRIP_PACKAGES:BOOL=${strip} && make package";
    }
}

sub check_rpm {
    # Run rpmlint on the packages.
    # The true can be dropped, when the errors are resolved.
    return "rpmlint *.rpm || true";
}

sub create_deb {    
    my $is_debug = $_[0];
    if (!($ENV{NMI_PLATFORM} =~ /(Debian8|Ubuntu14)/)) {
        # Use native packaging tool
        return dirname($0) . "/build_uw_deb.sh";
    } else {
        #Reconfigure cmake variables for native package build   
        my $command = get_cmake_args();
        my $strip = "ON"; if ($is_debug) { $strip = "OFF"; }
        return "$command -DCONDOR_PACKAGE_BUILD:BOOL=ON -DCONDOR_STRIP_PACKAGES:BOOL=${strip} && make VERBOSE=1 package";
    }
}

sub check_deb {
    # Run lintian on the packages.
    my $fail_on_error='--fail-on error';
    if ($ENV{NMI_PLATFORM} =~ /Ubuntu20/) {
        $fail_on_error='';
    }
    # Suppress changelog checks, daily builds trigger these
    my $suppress_tags='--suppress-tags latest-debian-changelog-entry-without-new-date,latest-changelog-entry-without-new-date';
    if ($ENV{NMI_PLATFORM} =~ /Ubuntu/) {
        # Ubuntu complains about the "unstable" distro
        $suppress_tags="$suppress_tags,bad-distribution-in-changes-file";
    }
    return "lintian $fail_on_error $suppress_tags *.changes";
}
