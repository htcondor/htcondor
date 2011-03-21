#!/bin/bash
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


#srb_root=../../externals/install/srb-3.2.1
srb_root=/p/condor/workspaces/externals/install/srb-3.2.1
export PATH=$srb_root/utilities/bin:$PATH
source /unsup/vdt/setup.sh

fail=0
tests=0

cleanup()
{
	rm -f termcap
	Sexit;Sinit -v
	Srm termcap
	Sexit
}

final_cleanup()
{
	cleanup
	if [ -d $srbdir.$$ ]; then
		mv $srbdir.$$ $srbdir
	fi
	printf "\nSummary: tests: %d fail: %d\n" $tests $fail
}

trap final_cleanup 0

echo "TEST: nominal file->srb-file roundtrip"
cleanup
Sinit -v

prog="./stork.transfer.srb"

src="file:/etc/termcap"
dest="srb://weber.condor:Wx12TpZk5@orion.sdsc.edu:8588/home/weber.condor/termcap"
cmd="$prog $src $dest"
echo $cmd
$cmd

echo "****************** after 1st xfer:"
Sls -l

dest="file:`pwd`/termcap"
src="srb://weber.condor:Wx12TpZk5@orion.sdsc.edu:8588/home/weber.condor/termcap"
cmd="$prog $src $dest"
echo $cmd
$cmd

echo "after 2nd xfer:"
ls -l termcap

tests=`expr $tests + 1`
if cmp /etc/termcap termcap; then
	echo test success
else
	echo TEST FAIL
	fail=`expr $fail + 1`
fi
cmd="sum /etc/termcap termcap"
echo $cmd
$cmd

# the following test still fails, and appears to be a bug in the srb-3.2.1 API
exit $fail

srbdir=$HOME/.srb
if [ -d $srbdir ]; then

	cleanup
	echo "TEST: null SRB connect parms"
	mv $srbdir $srbdir.$$

	src="file:/etc/termcap"
	dest="srb://orion.sdsc.edu/home/weber.condor/termcap"
	cmd="$prog $src $dest"
	echo $cmd
	$cmd
	status=$?
	echo status = $status
	tests=`expr $tests + 1`
	if [ $status -eq 1 ]; then
		echo test success
	else
		echo TEST FAIL
		fail=`expr $fail + 1`
	fi

	#setting environment
	cleanup
	echo "TEST: null SRB connect parms with environment"
	# This test still fails with:
	#	FATAL: clConnect: Unable to determine the client domainName!
	# Reason unknown.
	src="file:/etc/termcap"
	dest="srb://orion.sdsc.edu/home/weber.condor/termcap"
	cmd="$prog $src $dest"
	cmd="\
		env \
			srbPort=8588 \
			srbAuth=Wx12TpZk5 \
			srbUser=weber \
			mdasDomainHome=condor \
		$cmd"
	echo $cmd
	$cmd
	status=$?
	echo status = $status
	tests=`expr $tests + 1`
	if [ $status -eq 0 ]; then
		echo test success
	else
		echo TEST FAIL
		fail=`expr $fail + 1`
	fi


	mv $srbdir.$$ $srbdir
else
	echo SKIPPING SRB ENVIRONMENT TESTS
fi

exit $fail
