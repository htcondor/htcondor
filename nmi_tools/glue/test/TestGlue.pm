#!/usr/bin/env perl
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


sub print_debug_header {
    my $cwd = Cwd::getcwd();
    
    print "----------- Debug Header ----------------\n";
    print "Current time: " . scalar(localtime) . "\n";
    print "Current host: " . `/bin/hostname -f`;
    print "CWD: $cwd\n";
    print "Perl path: $^X\n";
    print "Perl version: $]\n";
    print "Uptime: " . `uptime`;
    dir_listing(".");
    print "---------- End Debug Header --------------\n";
}

sub setup_test_environment {

    my $base_dir = Cwd::getcwd();

    if( not is_windows() ) {
        set_env("BASE_DIR", $base_dir);
        set_env("PATH", "$base_dir/nmi_tools/glue/test:$base_dir/condor/bin:$base_dir/condor/sbin:$ENV{PATH}");
        set_env("CONDOR_CONFIG", "$base_dir/condor_tests/TestingPersonalCondor/condor_config");
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

        # Condor will want Win32-style paths for CONDOR_CONFIG
        set_env("CONDOR_CONFIG", "$base_dir\\condor_tests\\TestingPersonalCondor\\condor_config");
        
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

sub set_env {
    my ($key, $val) = @_;;
    print "Setting environment variable:\n";
    print "\t$key -> '$val'\n";
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

1;
