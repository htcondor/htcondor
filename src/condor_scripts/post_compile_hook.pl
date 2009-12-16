#! /usr/bin/env perl

###############################################################################
# This program implements various static analysis algorithms usually on
# object files produced during Condor builds. It allows us to implement
# certain kinds of code policy, such as symbol poisoning--e.g., you may not
# call libc's open().
###############################################################################

use Getopt::Long;
use strict;
use warnings;
use File::Basename;

use vars qw/ %args /;

$| = 1;

###############################################################################
# Various data structures which describe the semantics of operations of this
# script.
###############################################################################

# This data structure defines poisonous symbols and their extent.
my @poisoned_syms = (
	# NOTE: You must use the C ABI, meaning if you want to poison a C++/fortran
	# symbol, you must enter the C++/fortran symbol as if it were a C linkage
	# ABI.

	# Each of these are an annonymous hash table which represents one policy
	# for a set of symbols. This policy includes the specific .o files which
	# should be exempt including entire directories. Having multiple hash
	# tables in this structure allows each poisoned symbol group to be applied
	# only to where it needs to be without interfereing with other symbol
	# groups.
	{
		# The symbols which may not exist in any .o file...
		'poison' => ['open', 'open64', 'fopen',
                             '_open',   # OS X 10.5: open(2) -> _open$UNIX2003
                             '_fopen'   # OS X 10.5: fopen(3) -> _fopen
                            ],

		# except these files...
		'exempt' => [
			# This is the safe open implementation, obviously it needs to call
			# the poisoned function.
			'condor_c++_util/condor_open.o',
			'condor_c++_util/condor_open-pic.o',

			# needs review
			'condor_startd.V6/glexec_wrapper.o',
			'condor_c++_util/write_user_log.o',
			'condor_c++_util/write_user_log-pic.o',

			# used in test suite.
			'condor_c++_util/memory_file.o',

			# Not a Condor program, but a tool used by the build
			'condor_util_lib/y.tab.o',

			# This code is meant to be adapted by others, so it needs less
			# contraints in our handling of it. Also, this is a read only
			# fopen.
			'condor_chirp/chirp_client.o',

			# Comes out of the gsoap external and doesn't depend upon any
			# condor codes.
			'condor_amazon/wsseapi.o',

			# This is a test program which presumably doesn't need the poison
			# policy protection.
			'condor_lease_manager/lease_manager_tester.o',
		],

		# These are wholesale exempt since they represent code we know to be
		# partitioned from the rest of the codebase in terms of the poisoning
		# of this symbol set. I could probably implement the method by which
		# this is checked better, and using perl's module to rip apart paths.
		'exempt_dirs' => [
			'condor_privsep',
			'gt2_gahp',
			'cream_gahp',
			'nordugrid_gahp',
			'condor_ckpt',
			'condor_syscall_lib',
			'condor_tests',
			'gcc', # lives under condor_tests
			'gpp', # lives under condor_tests
			'g77', # lives under condor_tests
			'f90', # lives under condor_tests
			'f77', # lives under condor_tests
		],
	},
);

###############################################################################
# Everything below this line are functions which do the work of the script.
###############################################################################

sub parse_options
{
	my $rc;

	$rc = GetOptions(\%args,
		'help',
		'pwd=s',
		'abi=s',
		'arch=s',
		'platform=s',
		'distro=s',
		'source-file=s',
		'obj-file=s',
		'nm=s',
	);

	if (!$rc) {
		usage("You didn't specify some arguments properly!");
	}

	if (exists($args{'help'})) {
		usage();
	}

	if (!defined($args{'pwd'})) {
		usage("Please supply the fully qualified path to the link directory.");
	}

	if (!defined($args{'arch'})) {
		usage("Please supply the architecture of the compiled object file!");
	}

	if (!defined($args{'platform'})) {
		usage("Please supply the platform of the compiled object file!");
	}

	if (!defined($args{'distro'})) {
		usage("Please supply the distribution of the compiled object file!");
	}

	if (!defined($args{'source-file'})) {
		usage("Please supply the source-file name!");
	}

	if (!defined($args{'obj-file'})) {
		usage("Please supply the obj-file name!");
	}

	if (!defined($args{'nm'})) {
		if (-x "/usr/bin/nm") {
			$args{'nm'} = "/usr/bin/nm";
		}
	}
}

sub usage
{
	my ($msg) = @_;

	if (defined($msg)) {
		print "Error: $msg\n\n";
	}

	print <<EOF;
$0: <options>
--help         This usage.
--pwd          Path to where link is occurring.
--arch         Same as the \$(ARCH) Makefile variable
--platform     Same as the \$(PLATFORM) Makefile variable
--distro       Same as the \$(DISTRIBUTION) Makefile variable
--source-file  The basename of the sourcefile being compiled.
--obj-file     The basename of the object file produced form the sourcefile.
--nm           The path to an 'nm' like tool which we'll use to get symbol info.
EOF

	exit 1;
}

# The output of this function should be an array of "SYM,REF" lines where SYM
# is the symbol name with no whitespace around it, and REF is the source code
# location for the symbol, if any, with no whitespace around it (but there may
# be spaces in side it!). If no source location is determined then
# "SYM,unknown" is what is returned. If no symbols are found of this function
# isn't implemented, then return an empty array.
sub get_unreferenced_symbols_from_objfile
{
	my ($args) = @_;
	my @unref;

	# if we don't have any means of checking, then simply bail.
	if (!defined($args->{'nm'})) {
		print "WARNING: No 'nm' like program means poison policy can't " .
			"be enforced. Skipping.\n";
		return @unref;
	}

	# For now, this only works and has been tested on linux.
	if ($args->{'distro'} =~ /LINUX/) {
		# get the unreferenced symbols and where they were referenced.
		# nm is quite nice for this
		@unref = `$args->{'nm'} -u -l $args->{'obj-file'}`;
		canonicalize_nm_urefs(\@unref);
	} elsif ($args->{'distro'} =~ /OSX/) {
		# get the unreferenced symbols
		# nm on OS X does not provide line numbers
		@unref = `$args->{'nm'} -u $args->{'obj-file'}`;
		canonicalize_nm_urefs(\@unref);
	} else {
		print "WARNING: Poison policy not in effect on this platform.\n";
	}

	return @unref;
}

# This destructively modifies the array pointed to be the array reference
# passed in. This is suited for the style of references as denoted by
# a unix style 'nm' program.
sub canonicalize_nm_urefs
{
	my ($aref) = @_;

	# Convert each line from "\s*U\s+SYM\s+REF" to "SYM,REF" or "SYM,unknown"
	# if there isn't a source line reference for the symbol.  This takes into
	# account there may not be a \s+REF and if the REF portion has whitespace
	# embedded in the name.
	map {
		my $line = $_;
		my ($sym, $ref);

		# get rid of leading/ending whitespace and the ^U\s+
		$line =~ s/^\s+//g;
		$line =~ s/^U\s+//g;
		$line =~ s/\s+$//g;

		# There might or might not be a source reference for the symbol 
		$line =~ m/(^\w+)(\s+(.*$))?/;
		$sym = $1;
		$ref = $2;
		if (!defined($ref)) {
			$ref = "unknown";
		}
		# remove leading/ending whitespace
		$ref =~ s/^\s+//g;
		$ref =~ s/\s+$//g;

		# destructively update the line I'm working on to hold the right
		# value.
		$_ = "$sym,$ref";
	} @{$aref};
}

sub apply_poison_policy
{
	my ($args) = @_;
	my %psyms;
	my $s;
	my ($sym, $ref);
	my @usym;
	my $policy;
	my @poison;
	my @exempt;
	my @exempt_dirs;
	my $p;
	my $f;

	@usym = get_unreferenced_symbols_from_objfile($args);

	# if there are no unreferenced symbols, or we're on a platform where
	# we hadn't ported the get_unreferenced_symbols_from_objfile() function
	# yet, then just bail.
	if (scalar(@usym) == 0) {
		return;
	}

	# n^3 algorithm? Yikes.

	# We'll check each policy statement
	foreach $policy (@poisoned_syms) {
		@poison = @{$policy->{'poison'}};
		@exempt = @{$policy->{'exempt'}};
		@exempt_dirs = @{$policy->{'exempt_dirs'}};

		# skip anything in the exempt_dirs
		# XXX Implement this a little better to make exempt_dirs really mean
		# anything below that directory.
		next if (is_member(basename($args->{'pwd'}), \@exempt_dirs));

		# Check each unreferenced symbol against the policy
		foreach $s (@usym) {
			($sym, $ref) = split(/,/, $s);

			# Is the symbol a member of the poison set?
			if (is_member($sym, \@poison)) {
				# yes, did it not occur in an exempted file?
				$f = basename($args->{'pwd'}) . "/$args->{'obj-file'}";
				if (!is_member($f, \@exempt)) {

					# found a poisoned symbol in a file which is not exempt:
					print "[ERROR] Poisoned Symbol policy violation: " .
						"Found poisoned symbol '$sym' in " .
						"$f referenced at $ref!\n";

					# a little sneaky, removing this file, but it'll make the
					# build fail repeatedly if someone/something keeps typing 
					# make.
					print "rm -f $args->{'pwd'}/$args->{'obj-file'}\n";
					`rm -f $args->{'pwd'}/$args->{'obj-file'}`;

					# Make sure to exit with a failure to stop the make process.
					exit 1;
				}
			}
		}
	}
}

# Is x an element of X?
sub is_member
{
	my ($needle, $haystack) = @_;
	my @ret;

	@ret = grep {m/^\Q$needle\E$/; } @{$haystack};

	if (scalar(@ret) != 0) {
		return 1;
	}

	return 0;
}

sub main
{
	parse_options();

	apply_poison_policy(\%args);

	return 0;
}

exit main();

