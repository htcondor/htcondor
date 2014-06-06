#! /usr/bin/env perl
use strict;
use warnings;
use IPC::Open3;
use Time::HiRes qw(tv_interval gettimeofday);
use Archive::Tar;

BEGIN {
	if ($^O =~ /MSWin32/) {
		require Thread; Thread->import();
		require Thread::Queue; Thread::Queue->import();
	}
}

package CondorUtils;

our $VERSION = '1.00';
my $btdebug = 0;

use base 'Exporter';

our @EXPORT = qw(runcmd FAIL PASS ANY SIGNALED SIGNAL async_read verbose_system Which TRUE FALSE is_cygwin_perl is_windows is_windows_native_perl is_cygwin_perl fullchomp CreateDir CopyIt TarCreate TarExtract MoveIt DirLs List WhereIsInstallDir);

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
        my $date = strftime("%H:%M:%S", localtime);
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
		print "****** STDOUT ******\n";
		foreach my $thing1 (@outlines) {
			print "$thing1\n";
		}
		print "****** STDOUT ******\n";
		print "****** STDERR ******\n";
		foreach my $thing2 (@errlines) {
			print "$thing2\n";
		}
		print "****** STDERR ******\n";
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

#print "Which: looking for: $exe\n";
    if( is_windows_native_perl() ) {
		if($btdebug == 1) {
			print "windows native perl!\n";
		}
        return `\@for \%I in ($exe) do \@echo(\%~\$PATH:I`;
    }
	my $amwindows = is_windows();

	my @winpath = {};
	if($amwindows == 1) {
		if(is_cygwin_perl()) {
			@winpath = split /:/, $ENV{PATH};
			#$exe = "$exe" . ".exe";
		} else {
			@winpath = split /;/, $ENV{PATH};
			# should not get here
		}
	} else {
		@winpath = split /:/, $ENV{PATH};
	}

    foreach my $path (@winpath) {
        fullchomp( $path);
		#print "Checking <$path>\n";
        if(-f "$path/$exe") {
			if($btdebug == 1) {
				print "Which returning:$path/$exe\n";
			}
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

sub CreateDir
{
	my $cmdline = shift;
	my @argsin = split /\s/, $cmdline;
	my $cmdcount = @argsin;
	my $ret = 0;
	my $fullcmd = "";
	#print  "CreateDir: $cmdline argcout:$cmdcount\n";

	my $amwindows = is_windows();

	my $winpath = "";
	if($amwindows == 1) {
		if(is_windows_native_perl()) {
			# what if a linux path first?
			if($argsin[0] eq "-p") {
				shift @argsin;
			}
			foreach my $dir (@argsin) {
				$_ = $dir;
				s/\//\\/g;
				s/\\/\\\\/g;
				$dir = $_;
				if(-d "$dir") {
					next;
				}
				$fullcmd = "cmd /C mkdir $dir";
				$ret = system("$fullcmd");
				if($ret != 0) {
					print "THIS:$fullcmd Failed\n";
				}
			}
			return($ret);
		} else {
			if($argsin[0] eq "-p") {
				$winpath = `cygpath -w $argsin[1]`;
				CondorUtils::fullchomp($winpath);
				$_ = $winpath;
				s/\\/\\\\/g;
				$winpath = $_;
				if(-d "$argsin[1]") {
					return($ret);
				}
			} else {
				$winpath = `cygpath -w $argsin[0]`;
				CondorUtils::fullchomp($winpath);
				$_ = $winpath;
				s/\\/\\\\/g;
				$winpath = $_;
				if(-d "$argsin[0]") {
					return($ret);
				}
			}
		}

		$fullcmd = "cmd /C mkdir $winpath";
		$ret = system("$fullcmd");
		if($btdebug == 1) {
			print "Tried to create dir got ret value:$ret path:$winpath:$fullcmd\n";
		}
	} else {
		$fullcmd = "mkdir $cmdline"; 	
		if(-d $cmdline) {
			return($ret);
		}
		$ret = system("$fullcmd");
		#print "Tried to create dir got ret value:$ret path:$cmdline/$fullcmd\n";
	}
	return($ret);
}

sub List
{
    my $cmdline = shift;
	my @allargs = split /\s/, $cmdline;
    my $amwindows = is_windows();
    my $fullcmd = "";
	my $ret = 0;

    if($amwindows == 1) {
		$fullcmd = "cmd /C dir ";

		foreach my $patharg (@allargs) {
			if($patharg =~ /cygdrive/) {
				my $winpath = `cygpath -w $patharg`;
        		CondorUtils::fullchomp($winpath);
				$_ = $winpath;
        		s/\\/\\\\/g;
				$winpath = $_;
				$fullcmd = $fullcmd . " $winpath";
			} else {
				$fullcmd = $fullcmd . " $patharg";
	}
		}

		$ret = system("$fullcmd");
	} else {
		$fullcmd = "ls $cmdline";

		$ret = system("$fullcmd");
	}
	return($ret);
}

sub DirLs
{
    my $cmdline = shift;
    my $amwindows = is_windows();
    my $fullcmd = "";
	my $ret = 0;

    if($amwindows == 1) {
		$fullcmd = "cmd /C dir";

		$ret = system("$fullcmd");
	} else {
		$fullcmd = "pwd;ls";

		$ret = system("$fullcmd");
	}
	return($ret);
}


sub dir_listing {
	my (@path) = @_;

	my $path = File::Spec->catdir(@path);

	# If we have a relative path then show the CWD
	my $cwd = "";
	if(not File::Spec->file_name_is_absolute($path)) {
		$cwd = "(CWD: '" . Cwd::getcwd() . "')";
	}
	if($btdebug == 1) {
		print "Showing directory contents of path '$path' $cwd\n";
	}
    
	# We have to check $^O because the platform_* scripts will be executed on a Linux
	# submit node - but the nmi platform string will have winnt
	if( is_windows() && $^O ne "linux" ) {
		system("dir $path");
	} else {
		system("ls -l $path");
	}
}

sub CopyIt
{
    my $cmdline = shift;
    my $ret = 0;
    my $fullcmd = "";
    my $dashr = "no";
	#print  "CopyIt: $cmdline\n";
    my $winsrc = "";
    my $windest = "";

    my $amwindows = is_windows();
    if($cmdline =~ /\-r/) {
        $dashr = "yes";
        $_ = $cmdline;
        s/\-r//;
        $cmdline = $_;
    }
    # this should leave us source and destination
    my @argsin = split /\s/, $cmdline;
    my $cmdcount = @argsin;

    if($amwindows == 1) {
        $winsrc = `cygpath -w $argsin[0]`;
        $windest = `cygpath -w $argsin[1]`;
        CondorUtils::fullchomp($winsrc);
        CondorUtils::fullchomp($windest);
        $_ = $winsrc;
        s/\\/\\\\/g;
        $winsrc = $_;
        $_ = $windest;
        s/\\/\\\\/g;
        $windest = $_;
        $fullcmd = "xcopy $winsrc $windest /Y";
        if($dashr eq "yes") {
            $fullcmd = $fullcmd . " /s /e";
        }

        $ret = system("$fullcmd");
		#print "Tried to create dir got ret value:$ret cmd:$fullcmd\n";
    } else {
        $fullcmd = "cp ";
        if($dashr eq "yes") {
            $fullcmd = $fullcmd . "-r ";
        }
        $fullcmd = $fullcmd . "$cmdline";
        $ret = system("$fullcmd");
		#print "Tried to create dir got ret value:$ret path:$cmdline\n";
    }
    return($ret);
}

sub MoveIt
{
	# assume two args only
	my $cmdline = shift;
	my $ret = 0;
	my $fullcmd = "";
	#print  "MoveIt: $cmdline\n";
	my $winsrc = "";
	my $windest = "";

	my $amwindows = is_windows();

	# this should be source and destination
	my @argsin = split /\s/, $cmdline;
	my $cmdcount = @argsin;

	if($amwindows == 1) {
		$winsrc = `cygpath -w $argsin[0]`;
		$windest = `cygpath -w $argsin[1]`;
		CondorUtils::fullchomp($winsrc);
		CondorUtils::fullchomp($windest);
		$_ = $winsrc;
		s/\\/\\\\/g;
		$winsrc = $_;
		$_ = $windest;
		s/\\/\\\\/g;
		$windest = $_;
		$fullcmd = "cmd /C move $winsrc $windest";

		$ret = system("$fullcmd");
		#print "Tried to copy got ret value:$ret cmd:$fullcmd\n";
	} else {
		$fullcmd = "mv "; 	
		$fullcmd = $fullcmd . "$cmdline";
		$ret = system("$fullcmd");
		#print "Tried to create dir got ret value:$ret path:$cmdline\n";
	}
	return($ret);
}

sub TarCreate
{
}

sub TarExtract
{
	my $archive = shift;
	my $amwindows = is_windows();
	my $winpath = "";
	my $tarobject = Archive::Tar->new;
	if($amwindows == 1) {
		$winpath = `cygpath -w $archive`;
		CondorUtils::fullchomp($winpath);
		$archive = $winpath;
	}
	$tarobject->read($archive);
	$tarobject->extract();
}

sub WhereIsInstallDir {
	my $base_dir = shift;
	my $iswindows = shift;
	my $iscygwin = shift;
	my $iswindowsnativeperl = shift;

	my $installdir = "";
	my $wininstalldir = "";

	my $top = $base_dir;
	if($iswindows == 1) {
		my $top = Cwd::getcwd();
		if($btdebug == 1) {
			print "base_dir is \"$top\"\n";
		}
		if ($iscygwin) {
			my $crunched = `cygpath -m $top`;
			fullchomp($crunched);
			if($btdebug == 1) {
				print "cygpath changed it to: \"$crunched\"\n";
			}
			my $ppwwdd = `pwd`;
			if($btdebug == 1) {
				print "pwd says: $ppwwdd\n";
			}
		} else {
			my $ppwwdd = `cd`;
			if($btdebug == 1) {
				print"cd says: $ppwwdd\n";
			}
		}
	}

	my $master_name = "condor_master"; if ($iswindows) { $master_name = "condor_master.exe"; }
	my $tmp = Which($master_name);
	if ( ! ($tmp =~ /condor_master/ ) ) {
		print STDERR "Which($master_name) returned:$tmp\n";
		print STDERR "Unable to find a $master_name in your PATH!\n";
		system("ls build");
		print "PATH: $ENV{PATH}\n";
		exit(1);
	} else {
		if($btdebug == 1) {
			print "Found master via Which here:$tmp\n";
		}
	}
	fullchomp($tmp);
	if($btdebug == 1) {
		print "Install Directory \"$tmp\"\n";
	}
	if ($iswindows) {
		if ($iscygwin) {
			$tmp =~ s|\\|/|g; # convert backslashes to forward slashes.
			if($tmp =~ /^(.*)\/bin\/condor_master.exe\s*$/) {
				$installdir = $1;
				$tmp = `cygpath -m $1`;
				fullchomp($tmp);
				$wininstalldir = $tmp;
			}
		} else {
			$tmp =~ s/\\bin\\condor_master.exe$//i;
			$installdir = $tmp;
			$wininstalldir = $tmp;
		}
		$wininstalldir =~ s|/|\\|g; # forward slashes.to backslashes
		$installdir =~ s|\\|/|g; # convert backslashes to forward slashes.
		if($btdebug == 1) {
			print "Testing this Install Directory: \"$wininstalldir\"\n";
		}
	} else {
		$wininstalldir = "none";
		$tmp =~ s|//|/|g;
		if( ($tmp =~ /^(.*)\/sbin\/condor_master\s*$/) || \
				($tmp =~ /^(.*)\/bin\/condor_master\s*$/) ) {
			$installdir = $1;
			if($btdebug == 1) {
				print "Testing This Install Directory: \"$installdir\"\n";
			}
		} else {
			die "'$tmp' didn't match path RE\n";
		}
		if(defined $ENV{LD_LIBRARY_PATH}) {
			$ENV{LD_LIBRARY_PATH} = "$installdir/lib:$ENV{LD_LIBRARY_PATH}";
		} else {
			$ENV{LD_LIBRARY_PATH} = "$installdir/lib";
		}
		if(defined $ENV{PYTHONPATH}) {
			$ENV{PYTHONPATH} = "$installdir/lib/python:$ENV{PYTHONPATH}";
		} else {
			$ENV{PYTHONPATH} = "$installdir/lib/python";
		}
	}
	my $paths = "$installdir" . ",$wininstalldir";
	return($paths);
}
1;
