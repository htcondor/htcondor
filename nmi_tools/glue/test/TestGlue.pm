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

sub setup_test_environment {

    my $base_dir = Cwd::getcwd();

    if( not is_windows() ) {
        set_env("BASE_DIR", $base_dir);
        set_env("PATH", "$base_dir/nmi_tools/glue/test:$base_dir/condor/bin:$base_dir/condor/sbin:$ENV{PATH}");
        set_env("CONDOR_CONFIG", "$base_dir/condor_tests/TestingPersonalCondor/condor_config");
    }
    else {
        # Get the right slashes for Windows.  Apparently getcwd() returns forward slashes, even
        # on Windows.
        $base_dir =~ s|/|\\|g;
        set_env("BASE_DIR", $base_dir);

        # Windows requires the SystemRoot directory to the PATH.  This is generally C:\Windows.
        # Also, add the system32 subdirectory in this folder
        my $system_paths = "$ENV{SystemRoot}:" . File::Spec->catdir($ENV{SystemRoot}, "system32");

        set_env("PATH", "$base_dir\\nmi_tools\\glue\\test;$base_dir\\condor\\bin;C:\\tools:$system_paths");

        # Condor will want Win32-style paths for CONDOR_CONFIG
        set_env("CONDOR_CONFIG", "$base_dir\\condor_tests\\TestingPersonalCondor\\condor_config");
        
        # also, throw in the WIN32 version of the base directory path for later use
        set_env("WIN32_BASE_DIR", $base_dir);
    }
}



sub setup_task_environment {
    set_env("GCBTARGET", "nmi-s006.cs.wisc.edu");
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

    if( is_windows() ) {
        system("dir $path");
    }
    else {
        system("ls -l $path");
    }
}


sub which {
    my ($exe) = @_;

    if( is_windows() ) {
        return system('for /F %I in ("' . $exe . '") do echo %~$PATH:I');
    }
    else {
        return system("which $exe");
    }
}

sub is_windows {
    if( $ENV{NMI_PLATFORM} =~ /winnt/ ) {
        return 1;
    }
    return 0;
}

1;
