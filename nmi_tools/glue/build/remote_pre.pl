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
# script to setup the Condor build
######################################################################
use Cwd;

use POSIX 'strftime';
use Getopt::Long;
use vars qw/ $opt_use_externals_cache $opt_clear_externals_cache $opt_clear_externals_cache_weekly $opt_clear_externals_cache_daily /;
# We use -- and everything after it will be configure arguments only.
GetOptions(
            'use_externals_cache' => \$opt_use_externals_cache,
            'use-externals-cache' => \$opt_use_externals_cache,
            'clear_externals_cache' => \$opt_clear_externals_cache,
            'clear-externals-cache' => \$opt_clear_externals_cache,
            'clear_externals_cache_weekly' => \$opt_clear_externals_cache_weekly,
            'clear-externals-cache-weekly' => \$opt_clear_externals_cache_weekly,
            'clear-externals-cache-daily' => \$opt_clear_externals_cache_daily,
);

my $cmake = undef;
my $BaseDir = getcwd();
#my $SrcDir = "$BaseDir/src";
my $buildid_file = "BUILD-ID";
my $buildid;
my $gitsha_file = "GIT-SHA";
my $gitsha;
my $externals_loc="/scratch/condor_externals";
my $platform = "$ENV{NMI_PLATFORM}";
my %defines = (
    listvars => "-LA",
    #noregen => "-DCMAKE_SUPPRESS_REGENERATION:BOOL=TRUE",
    mirror => "-DEXTERNALS_SOURCE_URL:URL=https://parrot.cs.wisc.edu/externals",
    #mirror => "-DEXTERNALS_SOURCE_URL:URL=http://mirror.batlab.org/pub/export/externals",
    );

# autoflush our STDOUT
$| = 1;

# Streamlined Linux builds do not need remote_pre
if ($platform =~ m/AlmaLinux|AmazonLinux|CentOS|Debian|Fedora|openSUSE|Rocky|Ubuntu/) {
    print "remote_pre not needed for $platform, create_native does this.\n";
    exit 0; # cmake configuration is run as part of create_native
}
my $boos = 1; # build out of source
my $CloneDir = $BaseDir;
if ($boos) { $CloneDir =~ s/userdir/sources/; }

if ($ENV{NMI_PLATFORM} =~ /_win/i) {
	my $enable_vs9 = 0;
	my $enable_vs15 = 0;
	my $enable_vs16 = 0;
	my $enable_vs17 = 1;
	my $enable_x64 = 1;
	my $use_latest_vs = 1;
	my $use_cmake3 = 1;

	#uncomment to use vs9 on Win7 platform
	#if ($ENV{NMI_PLATFORM} =~ /Windows7/i) { $enable_vs9 = 1; }
	#if ($ENV{NMI_PLATFORM} =~ /Windows7/i) { $use_latest_vs = 1; $use_cmake3 = 1; }
	#if ($ENV{NMI_PLATFORM} =~ /Windows7/i) { $enable_vs17 = 1; $enable_vs16 = 0; $enable_vs15 = 0; }

	#uncomment to build x86 on Win8 platform (the rest of the build will follow this)
	#if ($ENV{NMI_PLATFORM} =~ /Windows8/i) { $enable_x64 = 0; }

	if ($enable_vs9 && $ENV{VS90COMNTOOLS} =~ /common7/i) {
		$defines{visualstudio} = '-G "Visual Studio 9 2008"';
		$ENV{PATH} = "$ENV{VS90COMNTOOLS}..\\IDE;$ENV{VS90COMNTOOLS}..\\..\\VC\\BIN;$ENV{PATH}";
	} elsif ($use_latest_vs) {
		if ($enable_vs17 && exists $ENV{VS170COMNTOOLS}) {
			$defines{visualstudio} = '-G "Visual Studio 17 2022"';
			#$ENV{PATH} = "$ENV{VS170COMNTOOLS}..\\..\\VC\\TOOLS\\MSVC\\14.31.31103\bin\hostx64\x64;$ENV{PATH}";
		} elsif ($enable_vs16 && exists $ENV{VS170COMNTOOLS}) {
			$defines{visualstudio} = '-G "Visual Studio 17 2022" -T v142';
			#$ENV{PATH} = "$ENV{VS170COMNTOOLS}..\\..\\VC\\TOOLS\\MSVC\\14.29.30133\bin\hostx64\x64;$ENV{PATH}";
		} elsif ($enable_vs15 && exists $ENV{VS170COMNTOOLS}) {
			$defines{visualstudio} = '-G "Visual Studio 17 2022" -T v141';
			#$ENV{PATH} = "$ENV{VS170COMNTOOLS}..\\..\\VC\\TOOLS\\MSVC\\14.16.27023\bin\hostx64\x64;$ENV{PATH}";
		} else {
			$defines{visualstudio} = '-G "Visual Studio 14 2015"';
			$ENV{PATH} = "$ENV{VS140COMNTOOLS}..\\IDE;$ENV{VS140COMNTOOLS}..\\..\\VC\\BIN;$ENV{PATH}";
			if ($enable_x64) { $defines{visualstudio} = '-G "Visual Studio 14 2015 Win64"'; }
		}
		$defines{python36} = "-DWANT_PYTHON36:BOOL=FALSE";
	} else {
		$defines{visualstudio} = '-G "Visual Studio 11"';
		$ENV{PATH} = "$ENV{VS110COMNTOOLS}..\\IDE;$ENV{VS110COMNTOOLS}..\\..\\VC\\BIN;$ENV{PATH}";
		if ($enable_x64) {
			$defines{visualstudio} = '-G "Visual Studio 11 Win64"';
		}
	}
    $externals_loc   = "c:/temp/condor";
    #$ENV{CONDOR_BLD_EXTERNAL_STAGE} = "$externals_loc";
	if ($use_cmake3) {
		if (-d "C:\\Tools\\CMake3\\bin") {
			$ENV{PATH} = "C:\\Tools\\CMake3\\bin;$ENV{PATH}";
		} else {
			$ENV{PATH} = "C:\\Program Files\\CMake3\\bin;$ENV{PATH}";
		}
	} else {
		$ENV{PATH} = "C:\\Program Files\\CMake 2.8\\bin;$ENV{PATH}";
	}

	# if not building Win64, change platform from x86_64_Windows to x86_Windows
	if ( ! $enable_x64) {
		print "Win64 not enabled and platform=$platform, fixing platform string...\n";
		$platform =~ s/_64_/_/;
	}
} elsif ($ENV{NMI_PLATFORM} =~ /macos/i) {
	# CRUFT Once 9.0 is EOL, remove the python.org version of python3
	#   from the mac build machines and remove this setting of PATH.
	# Ensure we're using the system python3
	$ENV{PATH} ="/usr/bin:$ENV{PATH}";
}
print "------------------------- ENV DUMP ------------------------\n";
foreach my $key ( sort {uc($a) cmp uc($b)} (keys %ENV) ) {
    print "$key=".$ENV{$key}."\n";
}
print "------------------------- ENV DUMP ------------------------\n";
print "Configure args: " . join(' ', @ARGV) . "\n";

######################################################################
# Determine the right cmake to use. Either the one on the machine is
# sufficient, or we download and build one. Either way, we know the
# absolute path to cmake.
######################################################################

if ($ENV{NMI_PLATFORM} =~ /_win/i) {
	$cmake = "cmake";
} else {
	$cmake = `which cmake3 2>&1`;
	if ($? != 0) {
		$cmake = `which cmake 2>&1`;
		if ($? != 0) {
			die "Cannot find a usable cmake install.";
		}
	}
	chomp $cmake;
}

print "Using cmake: $cmake\n";

# very useful when we land on windows if/when cygwin is in PATH
system("$cmake --version");

######################################################################
# figure out if we have access to ANALYZE option of VC++
######################################################################
if ($ENV{NMI_PLATFORM} =~ /_win/i) {
    my $prefast = `cl -?`;
    if ($prefast =~ /analyze/) { $prefast = 1; } else { $prefast = 0; }
    print "PREFAST = $prefast\n";
    if (1) {
       print "prefast will not be used on Batlab Win boxen because the machine(s) aren't keeping up.\n";
    } else {
       $defines{analyze} = "-DMSVC_ANALYZE:BOOL=$prefast";
    }
}

######################################################################
# figure out if we have java
######################################################################
if ($ENV{NMI_PLATFORM} =~ /_win/i) {
    my $javaver = `reg QUERY "HKLM\\Software\\JavaSoft\\Java Runtime Environment"`;
	#print "check for java runtime returned $javaver\n";
    # look for "CurrentVersion    REG_SZ    n.m"
    if ($javaver =~ /CurrentVersion\s+REG_SZ\s+([0-9\.]+)/) {
        print "Enabling java tests because java runtime version $1 was found\n";
    } else {
        print "Disabling java tests because no java runtime was found: $javaver\n";
        $defines{javatests} = "-DENABLE_JAVA_TESTS:BOOL=OFF";
    }
}

######################################################################
# grab out the build id so we can tell cmake what to use for buildid
######################################################################
print "Finding build id of Condor\n";
open( BUILDID, "$buildid_file" ) || die "Can't open $buildid_file: $!\n";
my @stat = stat(BUILDID);
while( <BUILDID> ) {
    chomp;
    $buildid = $_;
}
close( BUILDID );
if( ! $buildid ) {
    die "Can't find Condor build id in $buildid_file!\n";
}
print "Build id is: $buildid\n";
$defines{buildid} = "-DBUILDID:STRING=$buildid";

print "platform is: $platform\n";
$defines{platform} = "-DPLATFORM:STRING=$platform";
# all of the binaries we build on Windows should have Windows10 as the CondorPlatform
$platform =~ s/Windows[789]$/Windows10/;
$defines{condor_platform} = "-DCONDOR_PLATFORM:STRING=$platform";

######################################################################
# Grab the git sha if present
######################################################################
print "Finding Git SHA of Condor\n";
if( open( GITSHA, "$gitsha_file" ) ) {
    while( <GITSHA> ) {
        chomp;
        $gitsha = $_;
    }
    close( GITSHA );
    if( $gitsha ) {
        print "Git SHA is: $gitsha\n";
        $defines{gitsha} = "-DCONDOR_GIT_SHA:STRING=$gitsha";
    }
}

######################################################################
# deal with the externals cache
######################################################################
if ( defined( $opt_use_externals_cache ) ) {

	if (defined($opt_clear_externals_cache_weekly)) {
		print "Should we do weekly clearing of the externals cache?\n";
		@now = localtime;
		@then = localtime( (stat( $externals_loc ))[9] );
		if ( ($now[7] - $now[6]) != ($then[7] - $then[6]) ) {
			print "Doing weekly cache clearing\n";
			$opt_clear_externals_cache = 1;
		}
	}

	if (defined($opt_clear_externals_cache_daily)) {
		print "Should we do daily clearing of the externals cache?\n";
		@now = localtime;
		@then = localtime( (stat( $externals_loc ))[9] );
		if ( $then[7] < $now[7] || $then[5] < $now[5] ) {
			print "Doing daily cache clearing\n";
			$opt_clear_externals_cache = 1;
		}
	}

	if ( defined($opt_clear_externals_cache) ) {

		if ($ENV{NMI_PLATFORM} =~ /_win/i) {
			print "deleting externals cache\n";
			$exit_status = system("rd /s/q \"$externals_loc\"");
			if( $exit_status ) {
				die "failed with status $exit_status\n";
			}
		} else {
			print "deleting externals cache\n";
			$exit_status = system("rm -rf /scratch/condor_externals");

			if( $exit_status ) {
				die "failed with status $exit_status\n";
			}

			#system("mkdir -p $externals_loc");
			#system("chmod -f -R a+rwX $externals_loc");
		}

	}

	$defines{externals} = "-DCACHED_EXTERNALS:BOOL=TRUE";
} else {
	$defines{externals} = "-DCACHED_EXTERNALS:BOOL=FALSE";
}

if($platform =~ /Fedora1[789]$/) {
	$defines{python_version} = "-DPythonLibs_FIND_VERSION:STRING=2.7";
} if ($platform =~ /Fedora2[01]$/) {
	$defines{python_version} = "-DPythonLibs_FIND_VERSION:STRING=2.7";
}


######################################################################
# run cmake on the source tree
# Note: This stores the absolute path to cmake, which could be either
# on the machine, or in a prereq, or downloaded and built by our
# invocation of $installcmakecmd above.
######################################################################
if ($boos) {
	print "Doing out-of-source build, sources are at '$CloneDir'\n";
	if ($platform =~ /_win/i) {
		print "Converting '$CloneDir' to Windows style\n";
		$CloneDir =~ s/\//\\/g;
		print "Result: '$CloneDir'\n";
	} else {
	}
} else {
	$CloneDir = ".";
}

my @command = ( $cmake, "CMakeLists.txt",
	join(' ', values(%defines)),
	join(' ', @ARGV),
	$CloneDir
);
my $configexecstr = join( " ", @command );

print "$configexecstr\n";

#Store config string to file for later use (building native packages)
open (CMAKE_CONFIG, ">cmake_args.txt") || die "Cannot open cmake_args.txt: $!";
print CMAKE_CONFIG $configexecstr;
close (CMAKE_CONFIG);

$exit_status = system("$configexecstr 2>&1");

if( $exit_status ) {
    die "failed with status $exit_status\n";
}

exit 0;

