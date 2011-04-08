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

package TestGlue;

sub setup_test_environment {

    my $base_dir = Cwd::getcwd();
    set_env("BASE_DIR", $base_dir);

    if( $ENV{NMI_PLATFORM} !~ /winnt/ ) {
        set_env("PATH", "$base_dir/nmi_tools/glue/test:$base_dir/condor/bin:$base_dir/condor/sbin:$ENV{PATH}");
        set_env("CONDOR_CONFIG", "$base_dir/condor_tests/TestingPersonalCondor/condor_config");
    }
    else {
        # Get the right slashes for Windows.  Apparently getcwd() returns forward slashes, even
        # on Windows.
        (my $win_base_dir = $base_dir) =~ s|/|\\|g;

        set_env("PATH", "$base_dir\\nmi_tools\\glue\\test;$base_dir\\condor\\bin");

        # Condor will want Win32-style paths for CONDOR_CONFIG
        set_env("CONDOR_CONFIG", "$base_dir\\condor_tests\\TestingPersonalCondor\\condor_config");
        
        # also, throw in the WIN32 version of the base directory path for later use
        set_env("WIN32_BASE_DIR", $base_dir);
    }
}



sub setup_task_environment {
    $ENV{GCBTARGET} = "nmi-s006.cs.wisc.edu";
}


sub set_env {
    my ($key, $val) = @_;;
    print "Setting environment variable:\n";
    print "\t$key -> '$val'\n";
    $ENV{$key} = $val;
}

1;
