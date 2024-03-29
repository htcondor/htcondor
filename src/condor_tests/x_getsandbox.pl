#!/usr/bin/env perl
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

if(is_windows()){
	system("dir");
} else {
	system("ls -al");
}

my $request = $ARGV[0];
if($request eq "0")
{
	while(1) { sleep 1 };
}
else
{
	sleep $request;
}
exit(0);


sub is_windows {
    if (($^O =~ /MSWin32/) || ($^O =~ /cygwin/)) {
        return 1;
    }
    return 0;
}

