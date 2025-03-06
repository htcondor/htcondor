#!/usr/bin/env perl

use strict;
use warnings;

package SubmitInfo;

# (14:06:39) Nick Leroy: #715
# (14:07:09) Nick Leroy: /p/condor/workspaces/nleroy/nmi-ports

###############################################################################
# Note: Even though a lot of information is specified in this file, the various
# glue scripts can add/subtract/manipulate the arguments and or information
# based upon "details on the ground" in the glue script.  One example is the
# location of the --with-externals directory which is not specified in this
# file but instead figured out in the remote_pre script for the build glue.
# Generally, try to put your arguments here if they are really build or test
# specific, but if they are machine specific, then you'll probably have to put
# them into the actual glue script--of course, try to keep this to a minimum.
###############################################################################

###############################################################################
# Build/Test Sets
#
# The Key is a human readable name for a set of nmi_platforms that we would
# like to run a build and test on.
# 
# The Value is an array of nmi_platform names usually (but not required) to
# be described in %submit_info.
#
# Note: Included in the tests are the cross tests if defined.
#
# With nmi_condor_submit, one can request a build/test set to submit.
###############################################################################

# The sets of ports we know about natively in the glue script.
our %build_and_test_sets = (
	# The ports we officially support and for which we provide native binaries
	# on our download site.
	# If you don't specify what platforms you'd like built, then this is the
	# list to which we default.
	
	# NOTE: Keep the stable or developer release branches synchronized with
	# https://condor-wiki.cs.wisc.edu/index.cgi/wiki?p=DeveloperReleasePlan
	'official_ports' => [],
	
	# NMI will need builds on a set of platforms that we do not provide in our
	# core builds.	These are those platforms.
	'nmi_one_offs' => [],
	
	# We will build on a set of machines that we want to be sure continue building
	# but we do not release them so they are not included in the 'official ports' 
	# section above.  Platforms in this list include things like the latest Fedora
	# release - a build problem on this platform could indicate problems on a future
	# release of RHEL.
	'extra_builds' => [],
	
	'stduniv' => [
		'x86_64_deb_5.0',
		'x86_64_rhap_5',
		'x86_deb_5.0',
	],
	);

###############################################################################
# For every build, test, and cross test, of condor everywhere,
# these are the default prereqs _usually_ involved.
###############################################################################
my @default_prereqs = ();

###############################################################################
# Minimal build configuration
# 
# The build arguments to configure which will result in the smallest and most
# portable build of Condor.
#
# This array is being used to initialze key/value pairs in a hash table.
# That is why it has this creepy format.
###############################################################################
my @minimal_build_configure_args =
	(
	 '-DPROPER:BOOL'			 => 'OFF',
	 '-D_VERBOSE:BOOL'			  => 'ON',
	 '-DWITH_BLAHP:BOOL'		 => 'OFF',
	 '-DWITH_LIBVIRT:BOOL'		 => 'OFF',
	 '-DWITH_LIBXML2:BOOL'		 => 'OFF',
	 '-DWITH_VOMS:BOOL'			 => 'OFF',
	);

###############################################################################
# DEFAULT List of Tests to Run.
#
# This specifies the test suite testclasses (more than one if you'd like)
# which are run by default for any test. The default of 'quick' means the
# tests we expect to be run by default every day in the nightlies.
###############################################################################
my @default_testclass = ( 'quick' );

###############################################################################
# Default Test Suite Configure Arguments
#
# When running a test suite, this is the list of default arguments to pass to
# the test suite's configure.
###############################################################################
my @default_test_configure_args =
	(
	 '-DNOTHING_SPECIAL:BOOL' => 'ON',
	);

###############################################################################
# Default Configure Arguments for a Condor Build
#
# This assumes that configure will figure out on its own the right information.
# and is generally used for official or completed ports of Condor.
#
# This array is being used to initialze key/value pairs in a hash table.
# That is why it has this creepy format.
###############################################################################
my @default_build_configure_args =
	(
	 '-DPROPER:BOOL'	 => 'OFF',
	 '-D_VERBOSE:BOOL'	 => 'ON',
	);

############################################################
# Batlab new plaform names
#
# x86_64_Debian5
# x86_64_Debian6
# x86_64_Fedora15
# x86_64_Fedora16
# x86_64_Fedora17
# x86_64_MacOSX7
# x86_64_RedHat5
# x86_64_RedHat6
# x86_64_SL6
# x86_64_Ubuntu10
# x86_64_Ubuntu12
# x86_64_Windows7
# x86_Debian6
# x86_FreeBSD7
# x86_RedHat5
# x86_RedHat6
# x86_SL5
# x86_WindowsXP
##########################################################

###############################################################################
# Hash Table of Doom
#
# This table encodes the build and test information for each NMI platform.
###############################################################################
our %submit_info = (

	##########################################################################
	# Default platform chosen for an unknown platform.
	# This might or might not work. If it doesn't work, then likely
	# real changes to configure must be made to identify the platform, or
	# additional arguments specified to configure to set the arch, opsys, 
	# distro, etc, etc, etc.
	# A good sample of stuff to add in case it doesn't work is:
	# --with-arch=X86_64 \
	# --with-os=LINUX \
	# --with-kernel=2.6.18-194.3.1.el5 \		
	# --with-os_version=LINUX_UNKNOWN \				  
	# --with-sysname=unknown \
	# <the big pile of other arguments>
	# 
	# See:
	# https://condor-wiki.cs.wisc.edu/index.cgi/wiki?p=BuildingCondorOnUnix
	##########################################################################
	'default_minimal_platform'	=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Microsoft Windows 6.1/2000/xp/whatever on x86_64
	# This probably doesn't work--glue scripts do funky things with it.
	##########################################################################
	'x86_64_Windows7'		=> {
		'build' => {
			'configure_args' => { 
			  '-DCMAKE_SUPPRESS_REGENERATION:BOOL' => 'TRUE', # because the windows VM doesn't keep time very well.
		},
			'prereqs'	=> undef,
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> undef,
			'testclass' => [ @default_testclass ],
		},
	},
	
	'x86_64_Windows8'   => 'x86_64_Windows7',
	'x86_64_Windows9'   => 'x86_64_Windows7',
	'x86_64_Windows10'  => 'x86_64_Windows7',
	'x86_WindowsXP'		=> 'x86_64_Windows7',
	'x86_64_winnt_6.1'	=> 'x86_64_Windows7',
	'x86_winnt_5.1'		=> 'x86_64_Windows7',
	
	##########################################################################
	# Platform Debian 5.0 on x86_64
	##########################################################################
	'x86_64_Debian5'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [],
			'xtests'	=> [],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [],
			'testclass' => [ @default_testclass ],
		},
	},
	'x86_64_deb_5.0'	=> 'x86_64_Debian5',


	##########################################################################
	# Platform DEB 6 on x86_64 
	##########################################################################
	'x86_64_Debian6'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ ],
			'xtests'	=>	undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ ],
			'testclass' => [ @default_testclass ],
		},
	},
	'x86_64_deb_6.0'	=> 'x86_64_Debian6',

	##########################################################################
	# Platform DEB 7 on x86_64
	##########################################################################
	'x86_64_Debian7'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ ],
			'xtests'	=>	undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ ],
			'testclass' => [ @default_testclass ],
		},
	},

	'x86_64_Debian8'	        => 'x86_64_Debian7',
	'nmi-build:x86_64_Debian8'	=> 'x86_64_Debian7',

	##########################################################################
	# Platform DEB 9 on x86_64
	##########################################################################
	'x86_64_Debian9'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ ],
			'xtests'	=>	undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ ],
			'testclass' => [ @default_testclass ],
		},
	},
	'nmi-build:x86_64_Debian9'  => 'x86_64_Debian9',
	'nmi-build:x86_64_Debian10' => 'x86_64_Debian9',
	'nmi-build:x86_64_Debian11' => 'x86_64_Debian9',
	'nmi-build:x86_64_Debian12' => 'x86_64_Debian9',

	##########################################################################
	# Platform CentOS 8 on x86_64
	##########################################################################
	'x86_64_CentOS8'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ ],
			'xtests'	=>	undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ ],
			'testclass' => [ @default_testclass ],
		},
	},

	'nmi-build:x86_64_CentOS8' => 'x86_64_CentOS8',
	'nmi-build:x86_64_Rocky8' => 'x86_64_CentOS8',
	'nmi-build:aarch64_AlmaLinux8' => 'x86_64_CentOS8',
	'nmi-build:ppc64le_AlmaLinux8' => 'x86_64_CentOS8',
	'nmi-build:x86_64_AlmaLinux8' => 'x86_64_CentOS8',
	'nmi-build:aarch64_AlmaLinux9' => 'x86_64_CentOS8',
	'nmi-build:x86_64_AlmaLinux9' => 'x86_64_CentOS8',
	'nmi-build:aarch64_AlmaLinux10' => 'x86_64_CentOS8',
	'nmi-build:x86_64_AlmaLinux10' => 'x86_64_CentOS8',
	'nmi-build:x86_64_AmazonLinux2' => 'x86_64_Ubuntu18',

	##########################################################################
	# Platform RHEL 7 on x86_64
	##########################################################################
	'x86_64_RedHat7'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ ],
			'xtests'	=>	undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ ],
			'testclass' => [ @default_testclass ],
		},
	},

	'x86_64_CentOS7'	=> 'x86_64_RedHat7',
	'x86_64_SL7'		=> 'x86_64_RedHat7',
	'nmi-build:x86_64_CentOS7' => 'x86_64_RedHat7',

	# 32 bit CentOS 7
	'x86_CentOS7'		=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
                '-DWITH_PYTHON_BINDINGS:BOOL' => 'OFF',
            },
			'prereqs'	=> [ ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ ],
			'testclass' => [ @default_testclass ],
		},
	},

	'nmi-build:x86_CentOS7' => 'x86_CentOS7',

	##########################################################################
	# Platform RedHat and SL
	##########################################################################
	'x86_64_RedHat6'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> ['x86_64_SL6'],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass'	=> [ @default_testclass ],
		},
	},
	'x86_64_rhap_6.2'	=> 'x86_64_RedHat6',
	'x86_64_rhap_6.3'	=> 'x86_64_RedHat6',
	'x86_64_rhap_6.4'	=> 'x86_64_RedHat6',
	'x86_64_rhap_6.5'	=> 'x86_64_RedHat6',
	'x86_64_rhap_6.6'	=> 'x86_64_RedHat6',
	'x86_64_rhap_6.7'	=> 'x86_64_RedHat6',
	'x86_64_rhap_6.8'	=> 'x86_64_RedHat6',
	'x86_64_rhap_6.9'	=> 'x86_64_RedHat6',
	'nmi-build:x86_64_CentOS6'	=> 'x86_64_RedHat6',

	# Add the SWAMP's (temporary) platform name
	'swamp:rhel-6.4-64'	=> 'x86_64_RedHat6',

	# for now SL6 is the same as RedHat6
	'x86_64_SL6'	=> 'x86_64_RedHat6',
	'x86_64_sl_6.0' => 'x86_64_SL6',
	'x86_64_sl_6.1' => 'x86_64_SL6',
	'x86_64_sl_6.2' => 'x86_64_SL6',
	'x86_64_sl_6.3' => 'x86_64_SL6',

	# RedHat5
	'x86_64_RedHat5'		=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> [ ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, ],
			'testclass' => [ @default_testclass ],
		},
	},
	'x86_64_rhap_5.7'	=> 'x86_64_RedHat5',
	'x86_64_rhap_5.8'	=> 'x86_64_RedHat5',
	'x86_64_rhap_5.9'	=> 'x86_64_RedHat5',
	'x86_64_rhap_5.10'	=> 'x86_64_RedHat5', # dunno if 5.10 will actually be a release

	# 32 bit RedHat 6
	'x86_RedHat6'		=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> [ ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, ],
			'testclass' => [ @default_testclass ],
		},
	},
	'x86_rhap_6.2'		=> 'x86_RedHat6',
	'x86_rhap_6.3'		=> 'x86_RedHat6',
	'x86_rhap_6.4'		=> 'x86_RedHat6',
	'x86_rhap_6.5'		=> 'x86_RedHat6',
	'x86_rhap_6.6'		=> 'x86_RedHat6',
	'x86_rhap_6.7'		=> 'x86_RedHat6',
	'x86_rhap_6.8'		=> 'x86_RedHat6',
	'x86_rhap_6.9'		=> 'x86_RedHat6',
	'nmi-build:x86_CentOS6' => 'x86_RedHat6',

	# 32 bit RedHat 5
	'x86_RedHat5'		=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> [ ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, ],
			'testclass' => [ @default_testclass ],
		},
	},
	'x86_rhap_5.8'		=> 'x86_RedHat5',
	'x86_rhap_5.9'		=> 'x86_RedHat5',
	'x86_rhap_5.10'		=> 'x86_RedHat5',

	# 32 bit SL 5
	'x86_SL5' => {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ ],
			'testclass' => [ @default_testclass ],
		},
	},
	'x86_sl_5.7'	=> 'x86_SL5',
	'x86_sl_5.8'	=> 'x86_SL5',
	'x86_sl_5.9'	=> 'x86_SL5',
	'x86_sl_5.10'	=> 'x86_SL5',

	# Assume the two other SL platforms are identical to their RH cousins.
	'x86_64_SL5'	=> 'x86_64_RedHat5',
	'x86_SL6'		=> 'x86_RedHat6',

	##########################################################################
	# Platform MacOSX
	##########################################################################
	
	'x86_64_MacOSX' => {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass' => [ @default_testclass ],
		},
	},
	'x86_64_MacOSX15',	=> 'x86_64_MacOSX',
	'x86_64_macOS11',	=> 'x86_64_MacOSX',
	'x86_64_macOS13',	=> 'x86_64_MacOSX',

	#
	# The SWAMP platforms.
	#
	'swamp:debian-6.0.9-64-01-batlab'		=> 'x86_64_Debian6',
	'swamp:debian-7.5-64-01-batlab'			=> 'x86_64_Debian7',
	'swamp:rhel-5.10-32-01-batlab'			=> 'x86_RedHat5',
	'swamp:rhel-6.5-32-01-batlab'			=> 'x86_RedHat6',
	'swamp:rhel-5.10-64-01-batlab'			=> 'x86_64_RedHat5',
	'swamp:rhel-6.5-64-01-batlab'			=> 'x86_64_RedHat6',
	'swamp:ubuntu-12.04-64-01-batlab'		=> 'x86_64_Ubuntu12',
	'swamp:ubuntu-14.04-64-01-batlab'		=> 'x86_64_Ubuntu14',
	'swamp:fedora-19-64-01-batlab'			=> 'x86_64_Fedora19',
	'swamp:fedora-20-64-01-batlab'			=> 'x86_64_Fedora20',
	'swamp:scientific-5.10-32-01-batlab'	=> 'x86_SL5',
	'swamp:scientific-5.10-64-01-batlab'	=> 'x86_64_SL5',
	'swamp:scientific-6.5-32-01-batlab'		=> 'x86_SL6',
	'swamp:scientific-6.5-64-01-batlab'		=> 'x86_64_SL6',


	# These describe what a human, sadly, had to figure out about certain
	# ports that we do in a "one off" fashion. These ports are generally
	# not released to the public, but are often needed by NMI to run their
	# cluster. If by chance work happens on these ports to make them officially
	# released, then they will move from this section to the above section, 
	# and most likely with the above arguments to configure. Most of these
	# builds of Condor are as clipped as possible to ensure compilation.

	##########################################################################
	# Platform Fedora 15 on x86_64
	##########################################################################
	'x86_64_Fedora'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass' => [ @default_testclass ],
		},
	},
	'x86_64_Fedora15'				=> 'x86_64_Fedora',
	'x86_64_Fedora16'				=> 'x86_64_Fedora',
	'x86_64_Fedora17'				=> 'x86_64_Fedora',
	'x86_64_Fedora18'				=> 'x86_64_Fedora',
	'x86_64_Fedora19'				=> 'x86_64_Fedora',
	'x86_64_Fedora20'				=> 'x86_64_Fedora',
	'x86_64_Fedora21'				=> 'x86_64_Fedora',
	'x86_64_Fedora22'				=> 'x86_64_Fedora',
	'x86_64_Fedora23'				=> 'x86_64_Fedora',
	'x86_64_Fedora24'				=> 'x86_64_Fedora',
	'x86_64_Fedora25'				=> 'x86_64_Fedora',
	'x86_64_Fedora27'				=> 'x86_64_Fedora',
	'nmi-build:x86_64_Fedora27'		=> 'x86_64_Fedora',
	'nmi-build:x86_64_Fedora28'		=> 'x86_64_Fedora',
	'nmi-build:x86_64_Fedora29'		=> 'x86_64_Fedora',
	'nmi-build:x86_64_Fedora30'		=> 'x86_64_Fedora',
	'nmi-build:x86_64_Fedora31'		=> 'x86_64_Fedora',
	'nmi-build:x86_64_Fedora32'		=> 'x86_64_Fedora',
	'nmi-build:x86_64_Fedora38'		=> 'x86_64_Fedora',
	'nmi-build:x86_64_Fedora39'		=> 'x86_64_Fedora',
	'nmi-build:x86_64_Fedora40'		=> 'x86_64_Fedora',
	'nmi-build:x86_64_Fedora41'		=> 'x86_64_Fedora',
	'nmi-build:x86_64_AmazonLinux2023' => 'x86_64_Fedora',
	'nmi-build:x86_64_openSUSE15'   => 'x86_64_Fedora',
	
	'x86_64_fedora_15'				=> 'x86_64_Fedora',
	'x86_64_fedora_16'				=> 'x86_64_Fedora',
	'x86_64_fedora_17'				=> 'x86_64_Fedora',
	'x86_64_fedora_18'				=> 'x86_64_Fedora',
	'x86_64_fedora_19'				=> 'x86_64_Fedora',
	'x86_64_fedora_20'				=> 'x86_64_Fedora',
	'x86_64_fedora_21'				=> 'x86_64_Fedora',
	'x86_64_fedora_22'				=> 'x86_64_Fedora',
	'x86_64_fedora_23'				=> 'x86_64_Fedora',
	'x86_64_fedora_24'				=> 'x86_64_Fedora',
	'x86_64_fedora_25'				=> 'x86_64_Fedora',
	'x86_64_fedora_27'				=> 'x86_64_Fedora',
	'x86_64_fedora_28'				=> 'x86_64_Fedora',


	##########################################################################
	# Platform Ubuntu 10.04 on x86_64
	# This might work.
	##########################################################################
	'x86_64_Ubuntu10'		=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass' => [ @default_testclass ],
		},
	},
	'x86_64_ubuntu_10.04.4'		=> 'x86_64_Ubuntu10',

	##########################################################################
	# Platform Ubuntu 10.04 on x86
	##########################################################################
	'x86_64_Ubuntu10' => {
		'build' => {
			'configure_args' => { @minimal_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass' => [ @default_testclass ],
		},
	},
	'x86_64_ubuntu_10.04.4' => 'x86_64_Ubuntu10',

	'x86_64_Ubuntu12' => {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass' => [ @default_testclass ],
		},
	},

	'x86_64_Ubuntu14'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass'	=> [ @default_testclass ],
		},
	},

	'x86_64_Ubuntu16'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass'	=> [ @default_testclass ],
		},
	},

	'nmi-build:x86_64_Ubuntu16'	=> 'x86_64_Ubuntu16',

	'x86_64_Ubuntu18'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass'	=> [ @default_testclass ],
		},
	},

	'nmi-build:x86_64_Ubuntu18'	=> 'x86_64_Ubuntu18',
	'nmi-build:x86_64_Ubuntu20'	=> 'x86_64_Ubuntu18',
	'nmi-build:x86_64_Ubuntu22'	=> 'x86_64_Ubuntu18',
	'nmi-build:x86_64_Ubuntu24'	=> 'x86_64_Ubuntu18',
	'nmi-build:ppc64le_Ubuntu20'	=> 'x86_64_Ubuntu18',

	# Add the SWAMP's (temporary) platform name
	'swamp:ubuntu-12.04-64'					=> 'x86_64_Ubuntu12',

	##########################################################################
	# Platform openSUSE
	##########################################################################
	'x86_64_OpenSUSE11'				=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
								  '-DWITH_CURL:BOOL' => 'ON',
								  '-DWITH_LIBVIRT:BOOL' => 'ON',
								  '-DWITH_LIBXML2:BOOL' => 'ON',
			},
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => {
				@default_test_configure_args
				
			},
			'prereqs'	=> [ @default_prereqs ],
			'testclass'	=> [ @default_testclass ],
		},
	},
	'x86_64_opensuse_11.3'		=> 'x86_64_OpenSUSE11',
	'x86_64_opensuse_11.4'		=> 'x86_64_OpenSUSE11',

	##########################################################################
	# Platform FreeBSD
	##########################################################################
	'x86_FreeBSD7'		=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
				'-DENABLE_JAVA_TESTS:BOOL=OFF' => undef,
				'-DWITH_CURL:BOOL=OFF' => undef,
			},
			'prereqs'	=> [],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => {
				@default_test_configure_args
				
			},
			'prereqs'	=> [],
			'testclass'	=> [ @default_testclass ],
		},
	},
	'x86_freebsd_7.4'		=> 'x86_FreeBSD7',
	
	'x86_FreeBSD8'			=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
				'-DENABLE_JAVA_TESTS:BOOL=OFF' => undef,
				'-DWITH_CURL:BOOL=OFF' => undef,
			},
			'prereqs'	=> [ ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => {
				@default_test_configure_args
				
			},
			'prereqs'	=> [ ],
			'testclass'	=> [ @default_testclass ],
		},
	},
	'x86_64_freebsd_8.2'		=> 'x86_FreeBSD8',
);

while( 1 ) {
	my $fixed = 0;
	my @skipped;
	foreach my $platform (keys %submit_info) {
		next if ( ref($submit_info{$platform}) eq "HASH" );
		my $target = $submit_info{$platform};
		die "Self reference detected in '$platform' definition!!!"
			if ( $target eq $platform );
		if ( ! exists $submit_info{$target} ) {
			die "No matching platform '$target' for alias '$platform'";
		}
		if ( ref($submit_info{$target}) eq "HASH" )	 {
			$submit_info{$platform} = $submit_info{$target};
			$fixed++;
		}
		else {
			push( @skipped, $platform );
		}
	}
	last if ( scalar(@skipped) == 0 );
	if ( ! $fixed ) {
		die "Can't fixup ", join(", ",@skipped), " platforms!";
	}
}

###############################################################################
# The code below this comment is used to sanity check the above data and
# check for simple type errors and other typos. The main() function is run if
# this file is executed standalone, but not if it is 'use'd by another
# perl script. Of course, the functions are available for use in the other
# perl script if desired.
###############################################################################

sub typecheck {
	my $result = 1; # assume the typecheck passed.

	print "Running typecheck of submit_info.conf:\n";

	return $result;

	# An example, much more could be done...
	foreach my $p (sort keys %submit_info) {
		# ensure configure_args is present and defined
		if (!exists($submit_info{$p}{build}{configure_args})) {
			print "ERROR: Platform $p 'configure_args' not present\n";
			$result = 0;
		}
		# ensure 'configure_args' is defined properly.
		if (!defined($submit_info{$p}{build}{configure_args})) {
			print "ERROR: Platform $p 'configure_args' not defined\n";
			$result = 0;
		}

		# ensure configure_args is present and defined
		if (!exists($submit_info{$p}{test}{configure_args})) {
			print "ERROR: Platform $p 'configure_args' not present\n";
			$result = 0;
		}
		# ensure 'configure_args' is defined properly.
		if (!defined($submit_info{$p}{test}{configure_args})) {
			print "ERROR: Platform $p 'configure_args' not defined\n";
			$result = 0;
		}
	}

	# TODO: Add that the prereqs must contain unique entries.

	return $result;
}

# A simple thing explaining some statistics about submit_info. Mostly useful
# for grant work, I'm sure. :)
sub statistics {
	print "Statistics of submit_info:\n";

	my $nump = scalar(keys(%submit_info));

	print "\tNumber of nmi platforms described: $nump\n";

	#print "\tPlatforms:\n";
	#foreach my $p (sort keys %submit_info) {
	#	print "\t\t$p\n";
	#}
};

sub dump_info
{
	my ($f) = shift(@_);
	my ($bref, $tref);

	if (!defined($f)) {
		$f = *STDOUT;
	}
	
	print $f "-------------------------------\n";
	print $f "Dump of submit_info information\n";
	print $f "-------------------------------\n";

	foreach my $p (sort keys %submit_info) {
		my $found = 0;
		foreach my $pname ( @_ ) {
			if ( $pname eq $p ) {
				$found++;
				last;
			}
			elsif ( $pname =~ /\/(.*)\// ) {
				my $re = "$1";
				if ( $p =~ /$re/ ) {
					$found++;
					last;
				}
			}
		}
		next if ( ! $found );
		print $f "Platform: $p\n";

		print $f "Build Info:\n";
		if (!defined($submit_info{$p}{'build'})) {
			print $f "Undef\n";
		} else {
			$bref = $submit_info{$p}{'build'};

			# emit the build configure arguments
			print $f "\tConfigure Args: ";
			if (!defined($bref->{'configure_args'})) {
				print $f "Undef\n";
			} else {
				print $f join(' ', args_to_array($bref->{'configure_args'})) .
					"\n";
			}
			
			# emit the build prereqs
			print $f "\tPrereqs: ";
			if (!defined($bref->{'prereqs'})) {
				print $f "Undef\n";
			} else {
				print $f join(' ', @{$bref->{'prereqs'}}) . "\n";
			}

			# emit the cross tests
			print $f "\tXTests: ";
			if (!defined($bref->{'xtests'})) {
				print $f "Undef\n";
			} else {
				print $f join(' ', @{$bref->{'xtests'}}) . "\n";
			}
		}

		print $f "Test Info:\n";
		if (!defined($submit_info{$p}{'test'})) {
			print $f "Undef\n";
		} else {
			$tref = $submit_info{$p}{'test'};

			# emit the test configure arguments
			print $f "\tConfigure Args: ";
			if (!defined($tref->{'configure_args'})) {
				print $f "Undef\n";
			} else {
				print $f join(' ', args_to_array($tref->{'configure_args'})) .
					"\n";
			}
			
			# emit the test prereqs
			print $f "\tPrereqs: ";
			if (!defined($tref->{'prereqs'})) {
				print $f "Undef\n";
			} else {
				print $f join(' ', @{$tref->{'prereqs'}}) . "\n";
			}

			# emit the testclass
			print $f "\tTestClass: ";
			if (!defined($tref->{'testclass'})) {
				print $f "Undef\n";
			} else {
				print $f join(' ', @{$tref->{'testclass'}}) . "\n";
			}
		}

		print "\n";
	}
}

# This function converts (and resonably escapes) an argument hash into an array
# usually used for hashes like 'configure_args'
sub args_to_array {
	my ($arg_ref) = (@_);
	my @result;

	foreach my $k (sort keys %{$arg_ref}) {
		if (defined($arg_ref->{$k})) {
			push @result, "$k='" . $arg_ref->{$k} . "'";
		}
		else {
			push @result, "$k";
		}
	}
	return @result;
}

sub main
{
	my @platforms;
	my $usage = "usage: $0 [--help|-h] [--list|-l] [-a|--all] [(<platform>|/<regex>/) ...]";

	foreach my $arg ( @ARGV ) {
		if (  ( $arg eq "-l" ) or ( $arg eq "--list" ) ) {
			foreach my $key ( sort keys(%submit_info) ) {
				print "$key\n";
			}
			exit( 0 );
		}
		elsif (	 ( $arg eq "-a" ) or ( $arg eq "--all" ) ) {
			push( @platforms, "/.*/" );
		}
		elsif (	 ( $arg eq "-h" ) or ( $arg eq "--help" ) ) {
			print "$usage\n";
			print "	 --help|-h:	 This help\n";
			print "	 --list|-l:	 List available platforms\n";
			print "	 --all|-a:	 Dump info on all available platforms\n";
			print "	 <platform>: Dump info named platform\n";
			print "	 /<regex>/:	 Dump info on all platforms matching <regex>\n";
			exit(0);
		}
		elsif ( $arg =~ /_/ or $arg =~ /^\// ) {
			push( @platforms, $arg );
		}
		elsif ( $arg =~ /^-/ ) {
			print "Unknown option: '$arg'\n";
			print "$usage\n";
			exit( 1 );
		}
		else {
			print "'$arg' does not appear to be a valid platform name or regex\n";
			print "$usage\n";
			exit( 1 );
		}
	}
	if ( ! scalar(@platforms) ) {
		print "$usage\n";
		exit( 1 );
	}
	$#ARGV = -1;

	if (!typecheck()) {
		die "\tTypecheck failed!";
	} else {
		print "\tTypecheck passed.\n";
	}
	statistics();
	dump_info( undef, @platforms );
}

###############################################################################
# Topelevel code
###############################################################################

# The variable "$slaved_module" should be defined in the perl script
# that includes this perl file as a data initialization module. In this
# case, the main will not be run. It is useful to run submit_info.conf by hand
# after mucking with it and have it ensure that what you typed in made sense.
if (!defined($main::slaved_module)) {
	$main::slaved_module = undef; # silence warning
	main();
}

1;

### Local Variables: ***
### mode:perl ***
### tab-width: 4  ***
### End: ***
