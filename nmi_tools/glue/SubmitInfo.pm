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
	'official_ports' => [
		'x86_64_deb_6.0-updated', # this will switch to non-updated when NMI has that platform
		'x86_64_deb_5.0',
		'x86_64_rhap_5',
		'x86_64_rhas_3',
		'x86_deb_5.0',
		'x86_rhap_5',
		'x86_rhas_3',
		'x86_winnt_5.1',
		'x86_64_rhap_6.1-updated',
		'x86_64_macos_10.5-updated',
	],
	
	# NMI will need builds on a set of platforms that we do not provide in our
	# core builds.	These are those platforms.
	'nmi_one_offs' => [
		'x86_64_sol_5.10',
		'x86_64_sol_5.11',
		'x86_freebsd_7.4-updated',
		'x86_64_freebsd_8.2-updated',
	],
	
	# We will build on a set of machines that we want to be sure continue building
	# but we do not release them so they are not included in the 'official ports' 
	# section above.  Platforms in this list include things like the latest Fedora
	# release - a build problem on this platform could indicate problems on a future
	# release of RHEL.
	'extra_builds' => [
		'x86_64_rhas_4',
		'x86_64_fedora_14-updated',
		'x86_64_opensuse_11.4-updated',
		'x86_64_macos_10.6-updated',
	],
	
	'stduniv' => [
		'x86_64_deb_5.0',
		'x86_64_rhap_5',
		'x86_64_rhas_3',
		'x86_deb_5.0',
	],
	);

###############################################################################
# For every build, test, and cross test, of condor everywhere,
# these are the default prereqs _usually_ involved.
###############################################################################
my @default_prereqs = (
	'tar-1.14',
	'patch-2.5.4',
	'cmake-2.8.3',
	'flex-2.5.4a',
	'make-3.80',
	'byacc-1.9',
	'bison-1.25',
	'wget-1.9.1',
	'm4-1.4.1',
	);

# Hackery to test running in new batlab
if ((`hostname -f` eq "submit-1.batlab.org\n") ||
	(`hostname -f` eq "submit-2.batlab.org\n")) {
@default_prereqs = ();
} 

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
	 '-DCLIPPED:BOOL'			  => 'ON',
	 '-DWITH_BLAHP:BOOL'		 => 'OFF',
	 '-DWITH_BOOST:BOOL'		 => 'OFF',
	 '-DWITH_COREDUMPER:BOOL'	 => 'OFF',
	 '-DWITH_CREAM:BOOL'		 => 'OFF',
	 '-DWITH_DRMAA:BOOL'		 => 'OFF',
	 '-DWITH_GCB:BOOL'			 => 'OFF',
	 '-DWITH_GLOBUS:BOOL'		 => 'OFF',
	 '-DWITH_GSOAP:BOOL'		 => 'OFF',
	 '-DWITH_HADOOP:BOOL'		 => 'OFF',
	 '-DWITH_KRB5:BOOL'			 => 'OFF',
	 '-DWITH_LIBDELTACLOUD:BOOL' => 'OFF',
	 '-DWITH_LIBVIRT:BOOL'		 => 'OFF',
	 '-DWITH_LIBXML2:BOOL'		 => 'OFF',
	 '-DWITH_UNICOREGAHP:BOOL'	 => 'OFF',
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
	 #'-DSCRATCH_EXTERNALS:BOOL' => 'ON',
	);

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
	'x86_64_winnt_6.1'		=> {
		'build' => {
			'configure_args' => { 
			 # TJ 10/4/2011 new batlab can't handle quoted strings as args at the moment.
			 # '-G \"Visual Studio 9 2008\"' => undef,
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
	
	##########################################################################
	# Microsoft Windows 6.1/2000/xp/whatever on x86	
	# This probably doesn't work--glue scripts do funky things with it.
	##########################################################################
	'x86_winnt_6.1'		=> {
		'build' => {
			'configure_args' => { 
			 # TJ 10/4/2011 new batlab can't handle quoted strings as args at the moment.
			 # '-G \"Visual Studio 9 2008\"' => undef,
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

	##########################################################################
	# Microsoft Windows 5.1/2000/xp/whatever on x86_64
	# This probably doesn't work--glue scripts do funky things with it.
	##########################################################################
	'x86_64_winnt_5.1'	=> {
		'build' => {
			'configure_args' => { '-G \"Visual Studio 9 2008\"' => undef },
			'prereqs'	=> undef,
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> undef,
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Microsoft Windows 5.1/2000/xp/whatever on x86
	# the official "blessed" windows build configuration
	##########################################################################
	'x86_winnt_5.1'		=> {
		'build' => {
			'configure_args' => { '-G \"Visual Studio 9 2008\"' => undef },
			'prereqs'	=> [
				'cmake-2.8.3', '7-Zip-9.20', 'ActivePerl-5.10.1',
				'VisualStudio-9.0', 'WindowsSDK-6.1',
				],
				'xtests'		=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ ],
			'testclass' => [ @default_testclass ],
		},
	},
	
	##########################################################################
	# Microsoft Windows 5.1/2000/xp/whatever on x86
	# CMake build testing configuration
	##########################################################################
	'x86_winnt_5.1-tst' => {
		'build' => {
			'configure_args' => { '-G \"Visual Studio 9 2008\"' => undef },
			'prereqs'	=> undef,
			# when it works add x86_64_winnt_5.1 to the x_tests
			'xtests'	=> [ "x86_winnt_5.1", "x86_64_winnt_5.1", "x86_winnt_6.0" ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> undef,
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform Debian 5.0 on x86_64
	##########################################################################
	'x86_64_deb_5.0'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args,
								  '-DCLIPPED:BOOL' => 'OFF',
			},
			'prereqs'	=> [ 'libtool-1.5.26', 'cmake-2.8.3' ],
			'xtests'	=>	[ 'x86_64_ubuntu_10.04', ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ 'java-1.4.2_05' ],
			'testclass' => [ @default_testclass ],
		},
	},


	##########################################################################
	# Platform Debian 6.0 on x86_64 (updated)
	##########################################################################
	# This is the name of the platform in old batlab
	'x86_64_deb_6.0-updated' => {
		'build' => {
			'configure_args' => { @default_build_configure_args,
								  '-DCLIPPED:BOOL' => 'OFF',
			},
			'prereqs'	=> [ ],
			'xtests'	=>	[ 'x86_64_ubuntu_10.04', ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ 'java-1.4.2_05' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform DEB 6 on x86_64 
	##########################################################################
	# This is the name of the platform in new batlab
	# It is actually updated, despite what the name says
	'x86_64_deb_6.0' => {
		'build' => {
			'configure_args' => { @default_build_configure_args,
								  '-DCLIPPED:BOOL' => 'OFF',
			},
			'prereqs'	=> [ ],
			'xtests'	=>	[ 'x86_64_ubuntu_10.04', ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform RHEL 6 on x86
	##########################################################################
	'x86_rhap_6.0'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args,
				# turn this back on when ready
				# '-dclipped:bool=off' => undef,
			 },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.4.2_05' ],
			'testclass'	=> [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform RHEL 6 on x86. Unmanaged.
	##########################################################################
	'x86_rhap_6.0-updated'	=> 'x86_rhap_6.0',

	##########################################################################
	# Platform RHEL 6 on x86_64
	##########################################################################
	'x86_64_rhap_6.0'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args,
				'-DCLIPPED:BOOL' => 'OFF',
			 },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.4.2_05' ],
			'testclass'	=> [ @default_testclass ],
		},
	},

	'x86_64_rhap_6.0-updated'	=> 'x86_64_rhap_6.0',
	'x86_64_rhap_6.1-updated'	=> 'x86_64_rhap_6.0',

	# This is the new batlab one
	'x86_64_rhap_6.1'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args,
				'-DCLIPPED:BOOL' => 'OFF',
			 },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass'	=> [ @default_testclass ],
		},
	},

    'x86_64_sl_6.0' => 'x86_64_rhap_6.1',

	##########################################################################
	# Platform RHEL 5 on x86_64
	##########################################################################
	'x86_64_rhap_5'		=> {
		'build' => {
			'configure_args' => { @default_build_configure_args,
								  '-DCLIPPED:BOOL' => 'OFF',
			},
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> [ 
				'x86_64_fedora_14-updated',
				'x86_64_opensuse_11.4-updated',
				'x86_64_rhap_6.1-updated',
				],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.4.2_05' ],
			'testclass' => [ @default_testclass ],
		},
	},

	# This is the new batlab one
	'x86_64_rhap_5.7'		=> {
		'build' => {
			'configure_args' => { @default_build_configure_args,
								  '-DCLIPPED:BOOL' => 'OFF',
			},
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> [ ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, ],
			'testclass' => [ @default_testclass ],
		},
	},


	##########################################################################
	# Platform RHEL 3 on x86_64
	##########################################################################
	'x86_64_rhas_3'		=> {
		'build' => {
			'configure_args' => { @default_build_configure_args,
								  '-DCLIPPED:BOOL' => 'OFF',
								  '-DWITH_LIBCGROUP:BOOL' => 'OFF',
			},
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> [ ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.4.2_05', 'perl-5.8.5',
							 'VMware-server-1.0.7' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform Debian 5 on x86
	##########################################################################
	'x86_deb_5.0'		=> {
		'build' => {
			'configure_args' => { @default_build_configure_args,
								  '-DCLIPPED:BOOL' => 'OFF',
			},
									  'prereqs' => [ 'libtool-1.5.26', 'cmake-2.8.3' ],
									  'xtests'	=> [ ],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'		=> [ 'java-1.4.2_05' ],
			'testclass'	=> [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform Mac OSX 10.4 on x86
	##########################################################################
	'x86_macos_10.4'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs,
							 'coreutils-5.2.1',
							 'libtool-1.5.26',],
			'xtests'	=> [
				'x86_64_macos_10.6-updated',
				],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ 
				@default_prereqs, 
				'java-1.4.2_12', 
				'coreutils-5.2.1'
				],
				'testclass'		=> [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform Mac OSX 10.5 on x86_64
	# condor actually builds naturally for this one, we just don't release it
	##########################################################################
	'x86_64_macos_10.5' => {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [
				@default_prereqs,
				'libtool-1.5.26',
				],
				'xtests'		=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform Mac OSX 10.5 on x86_64 with updates
	# condor actually builds naturally for this one, we just don't release it
	##########################################################################
	'x86_64_macos_10.5-updated' => 'x86_64_macos_10.5',


	##########################################################################
	# Platform Mac OSX 10.6 on x86_64
	##########################################################################
	'x86_64_macos_10.6' => {
		'build' => {
			'configure_args' => {  @default_build_configure_args },
			'prereqs'	=> [
				@default_prereqs,
				'libtool-1.5.26',
				],
				'xtests'		=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform Mac OSX 10.6 with updates on x86_64
	##########################################################################
	'x86_64_macos_10.6-updated' => 'x86_64_macos_10.6',

	##########################################################################
	# Platform RHEL 5 on x86
	##########################################################################
	'x86_rhap_5'		=> {
		'build' => {
			'configure_args' => { @default_build_configure_args,
								  '-DCLIPPED:BOOL' => 'OFF',
			},
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> [ 
				'unmanaged-x86_rhap_5'
				],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.4.2_05' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform RHEL 3 on x86
	##########################################################################
	'x86_rhas_3'		=> {
		'build' => {
			'configure_args' => { @default_build_configure_args,
								  '-DCLIPPED:BOOL' => 'OFF',
								  '-DWITH_LIBCGROUP:BOOL' => 'OFF',
			},
			'prereqs'	=> [ 
				@default_prereqs,
				'perl-5.8.5', 'gzip-1.3.3', 'autoconf-2.59'
				],
				'xtests'		=> [ 
					'x86_rhas_4', 
					'x86_64_rhas_3',
					'x86_64_rhas_4',
				],
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.5.0_08', 'perl-5.8.5',
							 'VMware-server-1.0.7' ],
			'testclass' => [ @default_testclass ],
		},
	},


	# These describe what a human, sadly, had to figure out about certain
	# ports that we do in a "one off" fashion. These ports are generally
	# not released to the public, but are often needed by NMI to run their
	# cluster. If by chance work happens on these ports to make them officially
	# released, then they will move from this section to the above section, 
	# and most likely with the above arguments to configure. Most of these
	# builds of Condor are as clipped as possible to ensure compilation.

	##########################################################################
	# Platform Fedora 13 on x86_64
	##########################################################################
	'x86_64_fedora_13'	=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.5.0_08' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform Fedora 13 with updates on x86_64
	##########################################################################
	'x86_64_fedora_13-updated'	=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.5.0_08' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform Fedora 14 on x86_64
	##########################################################################
	'x86_64_fedora_14'	=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
			},
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
	# Platform Fedora 14 with updates on x86_64
	##########################################################################
	'x86_64_fedora_14-updated'	=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
			},
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.5.0_08' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform Solaris 11 on x86_64
	# Building openssl is problematic on this platform.	 There is
	# some confusion betwen 64-bit and 32-bit, which causes linkage
	# problems.	 Since ssh_to_job depends on openssl's base64 functions,
	# that is also disabled.
	##########################################################################
	'x86_64_sol_5.11'	=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
								  '-DWITH_OPENSSL:BOOL' => 'OFF',
								  '-DWITH_CURL:BOOL' => 'OFF',
								  '-DHAVE_SSH_TO_JOB:BOOL' => 'OFF',
								  '-DWITHOUT_SOAP_TEST:BOOL' => 'ON',
			},
			'prereqs'	=> [ @default_prereqs, 'perl-5.8.9', 'binutils-2.15',
							 'gzip-1.3.3', 'wget-1.9.1', 'coreutils-6.9',
				],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'perl-5.8.9', 'binutils-2.15',
							 'gzip-1.3.3', 'wget-1.9.1', 'coreutils-6.9' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform Solaris 10 on x86_64
	# Building openssl is problematic on this platform.	 There is
	# some confusion betwen 64-bit and 32-bit, which causes linkage
	# problems.	 Since ssh_to_job depends on openssl's base64 functions,
	# that is also disabled.
	##########################################################################
	'x86_64_sol_5.10'	=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
								  '-DWITH_OPENSSL:BOOL' => 'OFF',
								  '-DWITH_CURL:BOOL' => 'OFF',
								  '-DHAVE_SSH_TO_JOB:BOOL' => 'OFF',
								  '-DWITHOUT_SOAP_TEST:BOOL' => 'ON',
			},
			'prereqs'	=> [ @default_prereqs, 'perl-5.8.9', 'binutils-2.21',
							 'gzip-1.3.3', 'wget-1.9.1', 'coreutils-8.9',
				],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'perl-5.8.9', 'binutils-2.21',
							 'gzip-1.3.3', 'wget-1.9.1', 'coreutils-8.9' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform RHEL 5 on x86  (umanaged!)
	# This might work.
	##########################################################################
	'unmanaged-x86_rhap_5'		=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},
		
		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.5.0_08' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform RHEL 5.2 on X86_64
	# This might work.
	# I suspect this could be a real port if we bothered.
	##########################################################################
	'x86_64_rhap_5.2'	=> {
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
	# Platform RHEL 5.3 on X86_64
	# This might work.
	# I suspect this could be a real port if we bothered.
	##########################################################################
	'x86_64_rhap_5.3'	=> {
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
	# Platform RHEL 5.3 with updates on X86_64
	# This might work.
	# I suspect this could be a real port if we bothered.
	##########################################################################
	'x86_64_rhap_5.3-updated'	=> {
		'build' => {
			'configure_args' => { @default_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.4.2_05' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform RHEL 5.4 on X86_64
	# This might work.
	# I suspect this could be a real port if we bothered.
	##########################################################################
	'x86_64_rhap_5.4'	=> {
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
	# Platform SL 5.5 on X86_64
	# I suspect this could be a real port if we bothered.
	##########################################################################
	'x86_64_sl_5.5' => {
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
	# Platform RHEL 4 on X86_64
	# This might work.
	##########################################################################
	'x86_64_rhas_4'		=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
								  '-DWITH_LIBCGROUP:BOOL' => 'OFF',
			 },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.5.0_08', 'perl-5.8.9' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform SLES 9 on x86_64
	##########################################################################
	'x86_64_sles_9'				=> {
		'build' => {
			'configure_args' =>{ @minimal_build_configure_args },
			'prereqs'	=> [ @default_prereqs, 'wget-1.9.1' ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => {
				@default_test_configure_args,
				
			},
			'prereqs'	=> [ @default_prereqs, 'wget-1.9.1', 'java-1.4.2_05' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform Ubuntu 10.04 on x86_64
	# This might work.
	##########################################################################
	'x86_64_ubuntu_10.04'		=> {
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
	# Platform Ubuntu 10.04 on x86
	##########################################################################
	'x86_ubuntu_10.04' => {
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
	# Platform RHEL 4 on x86
	##########################################################################
	'x86_rhas_4'		=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args },
			'prereqs'	=> [ @default_prereqs ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => { @default_test_configure_args },
			'prereqs'	=> [ @default_prereqs, 'java-1.4.2_05', 'perl-5.8.5' ],
			'testclass' => [ @default_testclass ],
		},
	},

	##########################################################################
	# Platform openSUSE 11.3 on x86_64 (& updated)
	##########################################################################
	'x86_64_opensuse_11.3'				=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
								  '-DWITHOUT_SOAP_TEST:BOOL' => 'ON',
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
	'x86_64_opensuse_11.3-updated'		=> 'x86_64_opensuse_11.3',
	'x86_64_opensuse_11.4'				=> 'x86_64_opensuse_11.3',

	# This one is for old batlab with java and ruby test prereqs.
	# Can go away when old batlab does
	'x86_64_opensuse_11.4-updated'		=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
								  '-DWITHOUT_SOAP_TEST:BOOL' => 'ON',
								  '-DWITH_CURL:BOOL' => 'ON',
								  '-DWITH_LIBVIRT:BOOL' => 'ON',
								  '-DWITH_LIBXML2:BOOL' => 'ON',
			},
			'prereqs'	=> [ @default_prereqs, 'java-1.4.2_05' ],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => {
				@default_test_configure_args
				
			},
			'prereqs'	=> [ @default_prereqs, 'java-1.4.2_05' ],
			'testclass'	=> [ @default_testclass ],
		},
	},


	##########################################################################
	# Platform FreeBSD 7.4 on x86
	##########################################################################
	'x86_freebsd_7.4-updated'		=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
				'-DWITHOUT_SOAP_TEST:BOOL=ON' => undef,
				'-DENABLE_JAVA_TESTS:BOOL=OFF' => undef,
				'-DWITH_CURL:BOOL=OFF' => undef,
				'-DWITH_LIBVIRT:BOOL=OFF' => undef,
				'-DWITH_LIBXML2:BOOL=ON' => undef,
			},
			'prereqs'	=> [ 'tar-1.14',
							 'patch-2.6.1',
							 'cmake-2.8.3',
							 'flex-2.5.4a',
							 'make-3.80',
							 'byacc-1.9',
							 'bison-1.25',
							 'wget-1.9.1',
							 'm4-1.4.1',
				],
			'xtests'	=> undef,
		},

		'test' => {
			'configure_args' => {
				@default_test_configure_args
				
			},
			'prereqs'	=> [ 'tar-1.14',
							 'patch-2.6.1',
							 'cmake-2.8.3',
							 'flex-2.5.4a',
							 'make-3.80',
							 'byacc-1.9',
							 'bison-1.25',
							 'wget-1.9.1',
							 'm4-1.4.1',
				],
			'testclass'	=> [ @default_testclass ],
		},
	},
	'x86_64_freebsd_8.2'				=> {
		'build' => {
			'configure_args' => { @minimal_build_configure_args,
				'-DWITHOUT_SOAP_TEST:BOOL=ON' => undef,
				'-DENABLE_JAVA_TESTS:BOOL=OFF' => undef,
				'-DWITH_CURL:BOOL=OFF' => undef,
				'-DWITH_LIBVIRT:BOOL=OFF' => undef,
				'-DWITH_LIBXML2:BOOL=ON' => undef,
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
	'x86_64_freebsd_7.4-updated'		=> 'x86_freebsd_7.4-updated',
	'x86_freebsd_8.2-updated'			=> 'x86_freebsd_7.4-updated',
	'x86_64_freebsd_8.2-updated'		=> 'x86_freebsd_7.4-updated',
	'x86_64_freebsd_8.3'				=> 'x86_freebsd_7.4-updated',
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
