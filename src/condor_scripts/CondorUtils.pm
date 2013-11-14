#! /usr/bin/env perl
use strict;
use warnings;
use IPC::Open3;
use Time::HiRes qw(tv_interval gettimeofday);

BEGIN {
	if ($^O =~ /MSWin32/) {
		require Thread; Thread->import();
		require Thread::Queue; Thread::Queue->import();
	}
}

package CondorUtils;

our $VERSION = '1.00';

use base 'Exporter';

our @EXPORT = qw(runcmd FAIL PASS ANY SIGNALED SIGNAL async_read verbose_system Which TRUE FALSE is_cygwin_perl is_windows_native_perl fullchomp);

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
	#print "ANY called, return 1\n";
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
    	#	print "SIGNAL: Not found exsig=@exsig signaled=$signaled signal=$signal return=$ret\n";
		#} elsif ($matches == 1) {
    	#	print "SIGNAL: Found exsig=@exsig signaled=$signaled signal=$signal return=$ret\n";
		#} else {
    	#	die "SIGNAL: Errant regex specification, matched too many!";
		#}

		if($signaled && ($matches == 1)) {
			return( 1 );
		}

		return( 0 );
	}
}

# create a Queue, and a reader thread to read from $fh and write into the queue
# returns the Queue
sub async_reader {
	my $fh = shift;
	if ($^O =~ /MSWin32/) {
		my $Q  = new Thread::Queue;

		Thread->new(
			sub {
				$Q->enqueue($_) while <$fh>;
				$Q->enqueue(undef);
			}	
		)->detach;

		return $Q;
	} else {
		return undef;
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
	my $cmd = undef;
	my $t0 = 0.1;
	my $t1 = 0.1;
	my $signal = 0;
	my %returnthings;
	local(*IN,*OUT,*ERR);
	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
	my @abbr = qw( Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec );
        use POSIX qw/strftime/; # putting this at the top of the script doesn't work oddly
        my $date = strftime("%Y/%m/%d %H:%M:%S", localtime);
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

	my(%ACCEPTABLE_OPTIONS) = map {$_ => 1} qw(
		cmnt
		die_on_failed_expectation
		emit_output
		expect_result
		emit_string
		use_system
		sh_wrap
		);

	foreach my $key (keys %{$options}) {
		if(not exists $ACCEPTABLE_OPTIONS{$key}) {
			print "WARNING: runcmd called with unknown option $key. It will be ignored.\n";
		}
	}

	SetDefaults($options);
	#foreach my $key (keys %{$options}) {
		#print "${$options}{$key}\n";
		#print "$key\n";
	#}

	# some common *nix shell commands aren't valid on windows, but instead of fixing 
	# all of the tests, it's easier to just re-write the command stings here
	if ($^O =~ /MSWin32/) {
		if ($args =~ /^mkdir \-p/) {
			$args =~ s/^mkdir \-p/mkdir/;
			$args =~ s/\/cygwin\/c/c:/;
			$args =~ s/\//\\/g;
		}
	}

	my $rc = undef;
	my @outlines;
	my @errlines;

	if(${$options}{emit_output} == TRUE) {
		if(exists ${$options}{emit_string}) {
			PrintAddComment(${$options}{emit_string});
			PrintAltHeader();
		} else {
			PrintHeader();
		}
		PrintStart($date,$args);
	}


	$t1 = [Time::HiRes::gettimeofday];

	if(${$options}{use_system} == TRUE) {
		#print "Request to bypass open3<use_system=>TRUE>\n";
		$rc = system("$args");
		$t1 = [Time::HiRes::gettimeofday];
	} else {
		$childpid = IPC::Open3::open3(\*IN, \*OUT, \*ERR, $args);

		my $bulkout = "";
		my $bulkerror = "";

 		# ActiveState perl on Windows doesn't support select for files/pipes, but threaded IO works great
		if ($^O =~ /MSWin32/) {
			my $outQ = async_reader(\*OUT);
			my $errQ = async_reader(\*ERR);

			my $oe = 0; # set to 1 when OUT is EOF
			my $ee = 0; # set to 1 when ERR ie EOF

			while (1) {
				my $wait = 1;

				while ( ! $oe && $outQ->pending) {
					my $line = $outQ->dequeue or $oe = 1;
					if ($line) { $bulkout .= $line; }
					$wait = 0;
				}
				while ( ! $ee && $errQ->pending) {
					my $line = $errQ->dequeue or $ee = 1;
					if ($line) { $bulkerror .= $line; }
					$wait = 0;
				}

				if ($oe && $ee) { last; }
				if ($wait) { sleep(.1); }
			}

		} else {  # use select for Linux and Cygwin perl

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
		} # end while
		} # use select for Linux and cygwin perl

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

		die "ERROR: waitpid failed to reap pid $childpid!" 
			if ($childpid != waitpid($childpid, 0));
		$rc = $?;

		$t1 = [Time::HiRes::gettimeofday];
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
			PrintStdErr(\@errlines);
		}
		PrintFooter();
	}

	$returnthings{"success"} = $rc;
	$returnthings{"exitcode"} = $rc;
	$returnthings{"stdout"} = \@outlines;
	$returnthings{"stderr"} = \@errlines;

	#print "about to check expected result function signal <$signal> rc <$rc>\n";
	my $expected = ${$options}{expect_result}($signal, $signal, $rc, \@outlines, \@errlines);
	#my $expected = ${$options}{expect_result}();
	#print "expected returnval was <$expected>\n";
	$returnthings{"expectation"} = $expected;
	if(!$expected && (${$options}{die_on_failed_expectation} == TRUE)) {
		die "Expectation Failed on cmd <$args>\n";
	} else {
		#print "runcmd: Told Failure was OK\n";
	}
	return \%returnthings;
}

sub ProcessReturn {
	my ($status) = @_;
	my $rc = -1;
	my $signal = 0;
	my @result;

	#print "ProcessReturn: Entrance status " . sprintf("%x", $status) . "\n";
	if ($status == -1) {
		# failed to execute, how do I represent this? Choose -1 for now
		# since that is an impossible unix return code.
		$rc = -1;
		#print "ProcessReturn: Process Failed to Execute.\n";
	} elsif ($status & 0x7f) {
		# died with signal and maybe coredump.
		# Ignore the fact a coredump happened for now.

		# XXX Stupidly, we also make the return code the same as the 
		# signal. This is a legacy decision I don't want to change now because
		# I don't know how big the ramifications will be.
		$signal = $status & 0x7f;
		$rc = $signal;
		#print "ProcessReturn: Died with Signal: $signal\n";
	} else {
		# Child returns valid exit code
		$rc = $status >> 8;
		#print "ProcessReturn: Exited normally $rc\n";
	}

	#print "ProcessReturn: return=$rc, signal=$signal\n";
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

	# sh_wrap: wrap the arguments to runcmd with "/bin/sh -c ..."
	if(!(exists ${$options}{sh_wrap})) {
		${$options}{sh_wrap} = TRUE;
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

sub PrintAddComment
{
	my $message = shift;
	print "\n+-------------------------------------------------------------------------------\n";
	print "+ $message\n";
}

sub PrintAltHeader {
	print "+-------------------------------------------------------------------------------\n";
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


# which is broken on some platforms, so implement a Perl version here
sub Which {
    my ($exe) = @_;

    return "" if(!$exe);

    if( is_windows_native_perl() ) {
        return `\@for \%I in ($exe) do \@echo(\%~\$PATH:I`;
    }
    foreach my $path (split /:/, $ENV{PATH}) {
        chomp $path;
		#print "Checking <$path>\n";
        if(-f "$path/$exe") {
            return "$path/$exe";
        }
    }

    return "";
}

# Cygwin's chomp does not remove the \r
sub fullchomp {
    # Preserve the behavior of chomp, e.g. chomp $_ if no argument is specified.
    push (@_,$_) if( scalar(@_) == 0);

    foreach my $arg (@_) {
        $arg =~ s/[\012\015]+$//;
    }

    return;
}


sub is_windows {
    if (($^O =~ /MSWin32/) || ($^O =~ /cygwin/)) {
        return 1;
    }
    return 0;
}

sub is_cygwin_perl {
    if ($^O =~ /cygwin/) {
        return 1;
    }
    return 0;
}

sub is_windows_native_perl {
    if ($^O =~ /MSWin32/) {
         return 1;
    }
    return 0;
}

1;
