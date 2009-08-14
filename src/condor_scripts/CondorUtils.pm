#! /usr/bin/env perl
use strict;
use warnings;
use IPC::Open3;
use Time::HiRes qw(tv_interval gettimeofday);

package CondorUtils;

our $VERSION = '1.00';

use base 'Exporter';

our @EXPORT = qw(runcmd FAIL PASS ANY SIGNALED SIGNAL verbose_system TRUE FALSE);

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
		if($signaled && grep {$signal == $_} @exsig) {
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

	# is someone running a daemon  with  & open3 will hang forever collecting output
	my $donotwait = 0;
	if((index $args,"&") != -1) {
		$donotwait = 1;
	}
	# quill test getting schedd info
	if((index $args,"-direct schedd") != -1) {
		$donotwait = 1;
	}
	if((index $args,"-direct rdbms") != -1) {
		$donotwait = 1;
	}

	my $rc = 0;
	my @outlines;
	my @errlines;

	if(${$options}{emit_output} == TRUE) {
		PrintLine();
		PrintStart($date,$args);
	}

	if($donotwait) {
		$rc = system("$args");
	} else {
		$childpid = IPC::Open3::open3(\*IN, \*OUT, \*ERR, "/bin/sh -c \'$args\'");
	}

	$t1 = [Time::HiRes::gettimeofday];

	if($donotwait == 1) {
		# a task was started running in backgroud
	}else{
		waitpid($childpid, 0);
		@outlines = <OUT>;
		@errlines = <ERR>;
		close(OUT);
		close(ERR);

		$rc = $? & 0xffff;
	}

	my $elapsed = Time::HiRes::tv_interval($t0, $t1);
	my @returns = ProcessReturn($rc);

	$rc = $returns[0];
	$signal = $returns[1];
	$returnthings{"signal"} = $signal;

	if(${$options}{emit_output} == TRUE) {
		my $sz = $#outlines;
		if($sz != -1) {
			PrintStdOut(\@outlines);
		}
		$sz = $#errlines;
		if($sz != -1) {
			PrintStdOut(\@errlines);
		}
	}

	if(${$options}{emit_output} == TRUE) {
		PrintDone($rc, $signal, $elapsed);
		PrintLine();
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

}

sub PrintStdOut {
	my $arrayref = shift;
	if( defined @{$arrayref}[0]) {
		print "+ BEGIN STDOUT\n";
		foreach my $line (@{$arrayref}) {
			print "$line";
		}
		print "\n+ END STDOUT\n";
	}
}

sub PrintStdErr {
	my $arrayref = shift;
	if( defined @{$arrayref}[0]) {
		print "+ BEGIN STDERR\n";
		foreach my $line (@{$arrayref}) {
			print "$line";
		}
		print "\n+ END STDERR\n";
	}
}

sub PrintLine {
	print "+-----------------------------------------------------------------------------------\n";
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

1;
