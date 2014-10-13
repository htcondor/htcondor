#!/usr/bin/env pere
##**************************************************************
##
## Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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

use strict;
use warnings;
use Cwd;
use File::Spec;


package TestGlue;

use POSIX qw/strftime/;

use base 'Exporter';
use Net::Domain qw(hostfqdn);

our @EXPORT = qw(out is_windows is_cygwin_perl);

my $installdir = "";
my $wininstalldir = "";
my $initialconfig = "";

my $iswindows = is_windows();
my $iscygwin  = is_cygwin_perl();
my $configmain = "condor_examples/condor_config.generic";
my $configlocal = "condor_examples/condor_config.local.central.manager";
my $targetconfig = "";
my $targetconfiglocal = "";
my $want_core_dumps = 1;

BEGIN
{
}

sub print_debug_header {
    my $cwd = Cwd::getcwd();
    
    print "----------- Debug Header ----------------\n";
    print "Current time: " . scalar(localtime) . "\n";
    print "Current host: " . `/bin/hostname -f`;
    print "CWD: $cwd\n";
    print "Perl path: $^X\n";
    print "Perl version: $]\n";
    print "Uptime: " . `uptime`;
	print "PATH:$ENV{PATH}\n";
    dir_listing(".");
    print "---------- End Debug Header --------------\n";
}

sub setup_test_environment {

    my $base_dir = Cwd::getcwd();

    if( not is_windows() ) {
        set_env("BASE_DIR", $base_dir);
        set_env("PATH", "$base_dir/nmi_tools/glue/test:$base_dir/condor/bin:$base_dir/condor/sbin:$ENV{PATH}");
		set_env("TESTS","$base_dir/condor_tests");
        set_env("CONDOR_CONFIG", "$base_dir/condor/etc/condor_config");
		if(exists $ENV{LD_LIBRARY_PATH}) {
			set_env("LD_LIBRARY_PATH","$ENV{LD_LIBRARY_PATH}:$base_dir/condor/libexec:$base_dir/condor/lib:$base_dir/condor/lib/python");
		} else {
			set_env("LD_LIBRARY_PATH","$base_dir/condor/libexec:$base_dir/condor/lib:$base_dir/condor/lib/python");
		}
    }
    else {
        # Get the right slashes for Windows.  Apparently getcwd() returns forward slashes, even
        # on Windows.
        $base_dir =~ s|/|\\|g;
        set_env("BASE_DIR", $base_dir);

        # Create the Windows PATH
        my $front_path = "$base_dir\\nmi_tools\\glue\\test;$base_dir\\condor\\bin";

        # We installed some tools (unzip, tar) in C:\tools on our Windows NMI machines
        my $end_path = "C:\\tools";

        # We need to add Perl to the path (this works only in old-batlab)
        #$end_path .= ";C:\\prereq\\ActivePerl-5.10.1\\bin";

        my $force_cygwin = 1; # set to 0 to work on removing cygwin dependancies from tests.
        if ( $force_cygwin ) {
            if ( ! ($ENV{PATH} =~ /cygwin/i) ) {
                $front_path .= ";C:\\cygwin\\bin";
            }
        }

        # If we are not using cygwin perl, we need to have the msconfig tools in the path.
        if ( is_windows_native_perl() ) { $end_path = "$base_dir\\msconfig;$end_path"; }

        # Windows requires the SystemRoot directory to the PATH.  This is generally C:\Windows.
        # Also, add the system32 subdirectory in this folder
        #my $system_paths = "$ENV{SystemRoot};" . File::Spec->catdir($ENV{SystemRoot}, "system32");
        #$front_path .= ";$ENV{SystemRoot}";
        #$front_path .= ";" . File::Spec->catdir($ENV{SystemRoot}, "system32");

        set_env("PATH", "$front_path;$ENV{PATH};$end_path");
	# print "^^^^^^ Windows path set to:$ENV{PATH} ^^^^^^^^^^^^^^^^\n";

        # Condor will want Win32-style paths for CONDOR_CONFIG
		set_env("TESTS","$base_dir\\condor_tests");
		# CONDOR_CONFIG tried to be set in remote_pre, but can't do it then
        set_env("CONDOR_CONFIG", "$base_dir\\condor\\condor_config");
        
        # also, throw in the WIN32 version of the base directory path for later use
        set_env("WIN32_BASE_DIR", $base_dir);

        # full environment dump to help debugging
        if ( ! $force_cygwin ) {
            print "----------------------------------\n";
            print "Dumping environment:\n";
            system("set");
            print "----------------------------------\n\n";
        }
    }
}

sub WhereIsInstallDir {
	my $base_dir = shift;
	my $top = $base_dir;
	if($iswindows == 1) {
		my $top = Cwd::getcwd();
		print "base_dir is \"$top\"\n";
		if ($iscygwin) {
			my $crunched = `cygpath -m $top`;
			fullchomp($crunched);
			print "cygpath changed it to: \"$crunched\"\n";
			#my $ppwwdd = `pwd`;
			#print "pwd says: $ppwwdd\n";
		} else {
			#my $ppwwdd = `cd`;
			#print"cd says: $ppwwdd\n";
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
		print "Found master via Which here:$tmp\n";
	}
	fullchomp($tmp);
	print "Install Directory \"$tmp\"\n";
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
			print "$tmp before trying to remove bin\condor_master.exe\n";
			$tmp =~ s/\\bin\\condor_master.exe$//i;
			$installdir = $tmp;
			$wininstalldir = $tmp;
			print "$tmp after trying to remove bin\condor_master.exe\n";
			$tmp =~ s/\\bin\\condor_master.exe$//i;
		}
		$wininstalldir =~ s|/|\\|g; # forward slashes.to backslashes
		$installdir =~ s|\\|/|g; # convert backslashes to forward slashes.
		print "Testing this Install Directory: \"$wininstalldir\"\n";
		print "Installdir:$installdir\n";
	} else {
		$tmp =~ s|//|/|g;
		if( ($tmp =~ /^(.*)\/sbin\/condor_master\s*$/) || \
				($tmp =~ /^(.*)\/bin\/condor_master\s*$/) ) {
			$installdir = $1;
			print "Testing This Install Directory: \"$installdir\"\n";
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
}

sub set_env {
    my ($key, $val) = @_;;
    #print "Setting environment variable:\n";
    #print "\t$key -> '$val'\n";
    $ENV{$key} = $val;
}


sub dir_listing {
    my (@path) = @_;

    my $path = File::Spec->catdir(@path);

    # If we have a relative path then show the CWD
    my $cwd = "";
    if(not File::Spec->file_name_is_absolute($path)) {
        $cwd = "(CWD: '" . Cwd::getcwd() . "')";
    }
    print "Showing directory contents of path '$path' $cwd\n";

    # We have to check $^O because the platform_* scripts will be executed on a Linux
    # submit node - but the nmi platform string will have winnt
    if( is_windows() && $^O ne "linux" ) {
        system("dir $path");
    }
    else {
        system("ls -l $path");
    }
}


sub which {
    my ($exe) = @_;

if( is_windows() ) {
        return `\@for \%I in ($exe) do \@echo(\%~\$PATH:I`;
    }
    else {
        return system("which $exe");
    }
}

sub is_windows {
    if( $ENV{NMI_PLATFORM} =~ /_win/i ) {
        return 1;
    }
    return 0;
}

sub is_cygwin_perl {
    if ($^O =~ /cygwin/i) {
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

# Cygwin's chomp does not remove the \r
 sub fullchomp {
	# Preserve the behavior of chomp, e.g. chomp $_ if no argument is specified.
	push (@_,$_) if( scalar(@_) == 0);

	foreach my $arg (@_) {
 		$arg =~ s/[\012\015]+$//;
	}

	return;
}

##############################################################################
##
## getFqdnHost
##
## hostname sometimes does not return a fully qualified
## domain name and we must ensure we have one. We will call
## this function in the tests wherever we were calling
## hostname.
###############################################################################

sub getFqdnHost {
	my $host = hostfqdn();
	fullchomp($host);
	return($host);
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

sub CreateDir
{
	my $cmdline = shift;
	my @argsin = split /\s/, $cmdline;
	my $cmdcount = @argsin;
	my $ret = 0;
	my $fullcmd = "";
	print  "CreateDir: $cmdline argcout:$cmdcount\n";

	my $amwindows = is_windows();

	my $winpath = "";
	if($amwindows == 1) {
		if(is_windows_native_perl()) {
			if($argsin[0] eq "-p") {
				$_ = $argsin[1];
				s/\\/\\\\/g;
				$winpath = $_;
				#$winpath = $argsin[1];
				if(-d "$argsin[1]") {
					return($ret);
				}
			} else {
				#$_ = $argsin[0];
				#s/\\/\\\\/g;
				#$winpath = $_;
				$winpath = $argsin[0];
				if(-d "$argsin[0]") {
					return($ret);
				}
			}
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
		print "Tried to create dir got ret value:$ret path:$winpath/$fullcmd\n";
	} else {
		if($cmdline =~ /\-p/) {
			$_ = $cmdline;
			s/\-p//;
			$cmdline = $_;
		}
		$fullcmd = "mkdir $cmdline"; 	
		if(-d $cmdline) {
			return($ret);
		}
		$ret = system("$fullcmd");
		#print "Tried to create dir got ret value:$ret path:$cmdline/$fullcmd\n";
	}
	return($ret);
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

sub out {
    my ($msg) = @_;
    my $time = strftime("%H:%M:%S", localtime);
    print "$time: $msg\n";
}


1;
