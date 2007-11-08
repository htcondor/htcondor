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


use Cwd;

# This is a test to ensure that Condor properly cleans out the sandbox
# directory once the job is gone, even if the job does crazy things
# like create directories with permission 0555, etc.

$sandbox = getcwd();
print "sandbox: $sandbox\n";

mkdir( "layer1", 0777 ) || die "Can't mkdir(layer1): $!\n";
chdir( "layer1" ) || die "Can't chdir(layer1): $!\n";

`touch file1 file2`;
chmod( 0444, 'file1' ) || die "Can't chmod(0444, layer1/file1): $!\n";
chmod( 0444, 'file2' ) || die "Can't chmod(0444, layer1/file2): $!\n";

mkdir( "layer2", 0777 ) || die "Can't mkdir(layer2): $!\n";
chdir( "layer2" ) || die "Can't chdir(layer2): $!\n";

`touch file1 file2`;
chmod( 0444, 'file1' ) || die "Can't chmod(0444, layer2/file1): $!\n";
chmod( 0444, 'file2' ) || die "Can't chmod(0444, layer2/file2): $!\n";

chdir( ".." ) || die "Can't chdir(..): $!\n";

chmod( 0555, 'layer2' ) || die "Can't chmod(0555 layer2): $!\n";

exit(0);

