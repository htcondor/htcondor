#! /usr/bin/env perl
use strict;
use warnings;
use IPC::Open3;
use Time::HiRes qw(tv_interval gettimeofday);

package CondorUtils;

our $VERSION = '1.00';

use base 'Exporter';

our @EXPORT = qw(runcmd FAIL PASS ANY SIGNALED SIGNAL verbose_system Which TRUE FALSE);

sub TRUE{1};
sub FALSE{0};

####################################################################################
#
# subroutines used for RUNCMD
#
####################################################################################

# return 0 for false (expectation not met), 1 for true (expectation met)
sub PASS {
	my ($signaled, $signal, $ret) = @_;
	return 0 if !defined($ret);
	return 0 if $signaled;
	return $ret == 0;
}

# return 0 for false (expectation not met), 1 for true (expectation met)
sub FAIL {
	my ($signaled, $signal, $ret) = @_;
	return 1 if !defined($ret);
	return 1 if $signaled;
	return $ret != 0;
}

# return 0 for false (expectation not met), 1 for true (expectation met)
sub ANY {
	# always true
	return 1;
}

# return 0 for false (expectation not met), 1 for true (expectation met)
sub SIGNALED {
	my ($signaled, $signal, $ret) = @_;
	if($signaled) {
		return 1;
	} else {
		return 0;
	}
}

# return 0 for false (expectation not met), 1 for true (expectation met)
sub SIGNAL {
	my @exsig = @_;
	return sub
	{
		my ($signaled, $signal, $ret) = @_;
		my $matches = scalar(grep(/^\Q$signal\E$/, @exsig));

		#if ($matches == 0) {
    		#print "Not found\n";
		#} elsif ($matches == 1) {
    		#print "Found\n";
		#} else {
    		#die "Errant regex specification, matched too many!";
		#}

		if($signaled && ($matches == 1)) {
			return( 1 );
		}

		return( 0 );
	}
}

#others of interest may be:
#sub NOSIG {...} # can have any return code, just not a signal.
#sub SIG {...} # takes a list of signals you expect it to die with
#sub ANYSIGBUT {...} # must die with a signal, just not ones you specify
#
#But PASS AND FAIL are probably what you want immediately and if not
#supplied, then defaults to PASS. The returned structure should have whether
#or not the expected result happened.
#
#Of course, you'd call the expect_result function inside of the new_system()
#call.
#
#As for what happens of the expected event doesn't happen, you can supply
#{die_on_failed_expectation => 1} and that is set to true by default.
#
#With expect_result and die_on_failed_expectation, you can model something
#like running a command which might or might not fail, but you don't care
#and it won't kill the script.
#
#$ret = new_system("foobar 1 2 3 4", {expect_result => ANY});
#
#Since ANY always returns true, then you can't ever fail in the expectation
#of the result so the default die_on_failed_expectation => 1 will never fire.

sub runcmd {
	my $args = shift;
	my $options = shift;
	my $t0 = 0.1;
	my $t1 = 0.1;
	my $signal = 0;
	my %returnthings;
	local(*IN,*OUT,*ERR);
	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
	my @abbr = qw( Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec );
	my $realyear = sprintf("%02d", $year % 100);
	my $date = "$mon/$mday/$realyear $hour:$min:$sec";
	my $childpid;
	my $local_expectation = FALSE;
	my %altoptions;

	if( defined $options ) {
		#we are good
	} else {
		$options = \%altoptions;
		$local_expectation = TRUE;
	}
	$t0 = [Time::HiRes::gettimeofday];

	SetDefaults($options);
	#foreach my $key (keys %{$options}) {
		#print "${$options}{$key}\n";
		#print "$key\n";
	#}

	my $rc = 0;
	my @outlines;
	my @errlines;

	if(${$options}{emit_output} == TRUE) {
		PrintHeader();
		PrintStart($date,$args);
	}


	$t1 = [Time::HiRes::gettimeofday];

	if(${$options}{use_system} == TRUE) {
		print "Request to bypass open3<use_system=>TRUE>\n";
		$rc = system("$args");
		$t1 = [Time::HiRes::gettimeofday];
	} else {
		$childpid = IPC::Open3::open3(\*IN, \*OUT, \*ERR, "/bin/sh -c \'$args\'");
		$t1 = [Time::HiRes::gettimeofday];

		my $bulkout = "";
		my $bulkerror = "";
		my $tmpout = "";
		my $tmperr = "";
		my $rin = '';
		my $rout = '';
		my $readstdout = TRUE;
		my $readstderr = TRUE;
		my $readsize = 1024;
		my $bytesread = 0;

		while( $readstdout == TRUE || $readstderr == TRUE) {
			$rin = '';
			$rout = '';
			# drain data slowly
			if($readstderr == TRUE) {
				vec($rin, fileno(ERR), 1) = 1;
			}
			if($readstdout == TRUE) {
				vec($rin, fileno(OUT), 1) = 1;
			}
			my $nfound = select($rout=$rin, undef, undef, 0.5);
			if( $nfound != -1) {
				#print "select triggered $nfound\n";
				if($readstderr == TRUE) {
					if( vec($rout, fileno(ERR), 1)) {
						#print "Read err\n";
						$bytesread = sysread(ERR, $tmperr, $readsize);
						#print "Read $bytesread from stderr\n";
						if($bytesread == 0) {
							$readstderr = FALSE;
							close(ERR);
						} else {
							$bulkerror .= $tmperr;
						}
						#print "$tmperr\n";
					} 
				}
				if($readstdout == TRUE) {
					if( vec($rout, fileno(OUT), 1)) {
						#print "Read out\n";
						$bytesread = sysread(OUT, $tmpout, $readsize);
						#print "Read $bytesread from stdout\n";
						if($bytesread == 0) {
							$readstdout = FALSE;
							close(OUT);
						} else {
							$bulkout .= $tmpout;
						}
						#print "$tmpout\n";
					}
				}
			} else {
				print "Select error in runcmd:$!\n";
			}
		}

		#print "$bulkout\n";
		#print "\n++++++++++++++++++++++++++\n";
		$bulkout =~ s/\\r\\n/\n/g;
		#print "$bulkout\n";
		#print "\n++++++++++++++++++++++++++\n";
		@outlines = split /\n/, $bulkout;
		map {$_.= "\n"} @outlines;

		$bulkerror =~ s/\\r\\n/\n/g;
		@errlines = split /\n/, $bulkerror;
		map {$_.= "\n"} @errlines;

		waitpid($childpid, 0);
		$rc = $? & 0xffff;

	}

	my $elapsed = Time::HiRes::tv_interval($t0, $t1);
	my @returns = ProcessReturn($rc);

	$rc = $returns[0];
	$signal = $returns[1];
	$returnthings{"signal"} = $signal;

	if(${$options}{emit_output} == TRUE) {
		PrintDone($rc, $signal, $elapsed);
		if(exists ${$options}{cmnt}) {
			PrintComment(${$options}{cmnt});
		}
		my $sz = $#outlines;
		if($sz != -1) {
			PrintStdOut(\@outlines);
		}
		$sz = $#errlines;
		if($sz != -1) {
			PrintStdOut(\@errlines);
		}
		PrintFooter();
	}

	$returnthings{"success"} = $rc;
	$returnthings{"exitcode"} = $rc;
	$returnthings{"stdout"} = \@outlines;
	$returnthings{"stderr"} = \@errlines;

	my $expected = ${$options}{expect_result}($signal, $signal, $rc, \@outlines, \@errlines);
	$returnthings{"expectation"} = $expected;
	if(!$expected && (${$options}{die_on_failed_expectation} == TRUE)) {
		die "Expectation Failed on cmd <$args>\n";
	}
	return \%returnthings;
}

sub ProcessReturn {
	my $rc = shift;
	my $signal = 0;
	my @result;
	if ($rc == 0) {
		#print "ran with normal exit\n";
	} elsif ($rc == 0xff00) {
		#print "command failed: $!\n";
	} elsif (($rc & 0xff) == 0) {
		$rc >>= 8;
		#print "ran with non-zero exit status $rc\n";
	} else {
		#print "ran with ";
		if ($rc &   0x80) {
			$rc &= ~0x80;
			#print "coredump from ";
		}
		#print "signal $rc\n";
		$signal = $rc;
	}
	push @result, $rc;
	push @result, $signal;
	return @result;
}

sub SetDefaults {
	my $options = shift;

	# expect_result
	if(!(exists ${$options}{expect_result})) {
		${$options}{expect_result} = \&PASS;
	}

	# die_on_failed_expectation
	if(!(exists ${$options}{die_on_failed_expectation})) {
		${$options}{die_on_failed_expectation} = TRUE;
	}

	# emit_output
	if(!(exists ${$options}{emit_output})) {
		${$options}{emit_output} = TRUE;
	}

	# use_system
	if(!(exists ${$options}{use_system})) {
		${$options}{use_system} = FALSE;
	}

}

sub PrintStdOut {
	my $arrayref = shift;
	if( defined @{$arrayref}[0]) {
		print "+ BEGIN STDOUT\n";
		foreach my $line (@{$arrayref}) {
			print "$line";
		}
		print "+ END STDOUT\n";
	}
}

sub PrintStdErr {
	my $arrayref = shift;
	if( defined @{$arrayref}[0]) {
		print "+ BEGIN STDERR\n";
		foreach my $line (@{$arrayref}) {
			print "$line";
		}
		print "+ END STDERR\n";
	}
}

sub PrintHeader {
	print "\n+-------------------------------------------------------------------------------\n";
}

sub PrintFooter {
	print "+-------------------------------------------------------------------------------\n";
}

sub PrintComment {
	my $comment = shift;
	print "+ COMMENT: $comment\n";
}

sub PrintStart {
	my $date = shift;
	my $args = shift;

	print "+ CMD[$date]: $args\n";
}

sub PrintDone {
	my $rc = shift;
	my $signal = shift;
	my $elapsed = shift;

	my $final = " SIGNALED: ";
	if($signal != 0) {
		$final = $final . "YES, SIGNAL $signal, RETURNED: $rc, TIME $elapsed ";
	} else {
		$final = $final . "NO, RETURNED: $rc, TIME $elapsed ";
	}
	print "+$final\n";
}

####################################################################################
#
# subroutines used for VERBOSE_SYSTEM
#
####################################################################################

sub verbose_system {
	my $cmd = shift;
	my $options = shift;
	my $hashref = runcmd( $cmd, $options );
	return ${$hashref}{exitcode};
}

# Sometimes `which ...` is just plain broken due to stupid fringe vendor
# not quite bourne shells. So, we have our own implementation that simply
# looks in $ENV{"PATH"} for the program and return the "usual" response found
# across unicies. As for windows, well, for now it just sucks.

sub Which
{
	my $exe = shift(@_);

	if(!( defined  $exe)) {
		return "CP::Which called with no args\n";
	}
	my @paths;
	my $path;

	if( exists $ENV{PATH}) {
		@paths = split /:/, $ENV{PATH};
		foreach my $path (@paths) {
			chomp $path;
			if (-x "$path/$exe") {
				return "$path/$exe";
			}
		}
	} else {
		#print "Who is calling CondorPersonal::Which($exe)\n";
	}

	return "$exe: command not found";
}

# Sometimes `which ...` is just plain broken due to stupid fringe vendor
# not quite bourne shells. So, we have our own implementation that simply
# looks in $ENV{"PATH"} for the program and return the "usual" response found
# across unixies. As for windows, well, for now it just sucks, but it appears
# to at least work.

#BEGIN {
## A variable specific to the BEGIN block which retains its value across calls
## to Which. I use this to memoize the mapping between unix and windows paths
## via cygpath.
#my %memo;
#sub Which
#{
#	my $exe = shift(@_);
#	my $pexe;
#	my $origpath;

#	if(!( defined  $exe)) {
#		return "CT::Which called with no args\n";
#	}
#	my @paths;

#	# On unix, this does the right thing, mostly, on windows we are using
#	# cygwin, so it also mostly does the right thing initially.
#	@paths = split /:/, $ENV{PATH};

#	foreach my $path (@paths) {
#		fullchomp($path);
#		$origpath = $path;

#		# Here we convert each path to a windows path 
#		# before we use it with cygwin.
#		if ($iswindows) {
#			if (!exists($memo{$path})) {
#				# XXX Stupid slow code.  The right solution is to abstract the
#				# $ENV{PATH} variable and its cygpath converted counterpart and
#				# deal with said abstraction everywhere in the codebase.  A
#				# less right solution is to memoize the arguments to this
#				# function call. Guess which one I chose.
#				my $cygconvert = `cygpath -m -p "$path"`;
#				fullchomp($cygconvert);
#				$memo{$path} = $cygconvert; # memoize it
#				$path = $cygconvert;
#			} else {
#				# grab the memoized copy.
#				$path = $memo{$path};
#			}

#			# XXX Why just for this and not for all names with spaces in them?
#			if($path =~ /^(.*)Program Files(.*)$/){
#				$path = $1 . "progra~1" . $2;
#			} else {
#				CondorTest::debug("Path DOES NOT contain Program Files\n",3);
#			}
#		}

#		$pexe = "$path/$exe";

#		if ($iswindows) {
#			# Stupid windows, do this to ensure the -x works.
#			$pexe =~ s#/#\\#g;
#		}

#		if (-x "$pexe") {
#			# stupid caller code expects the result in unix format".
#			return "$origpath/$exe";
#		}
#	}

#	return "$exe: command not found";
#}
#}


1;
