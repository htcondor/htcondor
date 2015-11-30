#!/bin/sh

##**************************************************************
##
## Copyright (C) 1990-2015, Condor Team, Computer Sciences Department,
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


# Test to see if the kernel OOM kills small processes that use
# a lot of page cache.  Requires 2.5 Gb free in the current directory.

mypid=$$

cgroup_root=`cat /proc/mounts | grep ^cgroup | grep memory | awk '{print $2}'`

if [ "$cgroup_root"x = "x" ]
then
	echo "Cgroup file system is not mounted, exiting"
	exit 1
fi

echo "cgroup_root is $cgroup_root"

echo "Making new cgroup for testing: $cgroup_root/testing"

group=$cgroup_root/testing
mkdir $group

echo "Moving shell to testing cgroup"
echo $mypid >  $group/tasks

echo "Limiting testing cgroup to 100 Mb"
echo "100000000" > $group/memory.limit_in_bytes
echo "100000000" > $group/memory.memsw.limit_in_bytes

echo "Running dd..."
dd if=/dev/zero of=big_file bs=8M count=1024 > /dev/null 2>&1

result=$?
if [ $result = 0 ]
then
	echo "Success: cgroup is correctly using page cache"
else
	echo "Failure: cgroup is killing processes that do too much file io"
fi

rm -f big_file
# Remove self from cgroup so I can remove testing cgroup
echo $mypid > $group/../tasks
rmdir $group
exit 0
