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

# Stork, CredD common test harness variables, functions.

# Configuration constants

# Verify some prerequisite test files
DEVZERO=/dev/zero
DEVNULL=/dev/null

# Test enables
MEMORY_TESTS=true	# run memory tests
LEAK_TESTS=false	# run memory leak tests.  implies MEM_TESTS

# Optional memory debugger harness. Uncomment VALGRIND definition to enable
# memory debugging.
VALGRIND=~nleroy/local/bin/valgrind		# FIXME
#VALGRIND_OPTS="$VALGRIND_OPTS --verbose"
VALGRIND_OPTS="$VALGRIND_OPTS --tool=addrcheck"
VALGRIND_OPTS="$VALGRIND_OPTS --time-stamp=yes"
#VALGRIND_OPTS="$VALGRIND_OPTS --trace-children=yes"
#VALGRIND_OPTS="$VALGRIND_OPTS --show-below-main=yes"
VALGRIND_OPTS="$VALGRIND_OPTS --num-callers=60"
VALGRIND_LEAK_OPTS="$VALGRIND_LEAK_OPTS --leak-check=yes"
VALGRIND_LEAK_OPTS="$VALGRIND_LEAK_OPTS --show-reachable=yes"	#too noisy
VALGRIND_LEAK_OPTS="$VALGRIND_LEAK_OPTS --freelist-vol=5000000"
#VALGRIND_OPTS="$VALGRIND_OPTS --gen-suppressions=yes" # blocking promtp to tty
VALGRIND_LEAK_SUPPRESS=vg_leak.supp
VALGRIND_OPTS="$VALGRIND_OPTS --suppressions=$VALGRIND_LEAK_SUPPRESS"
#VALGRINDCMD="$VALGRIND $VALGRIND_OPTS"

#CREDD_HOST=localhost	# CredD host
CREDD_HOST=`hostname`	# CredD host
CREDD_START_DELAY=4	# daemon startup/shutdown delay

# credd daemon invocation
CREDD=condor_credd
CREDD_OPTS="$CREDD_OPTS -f"

# store_cred invocation
CRED_STORE=stork_store_cred
CRED_STORE_OPTS="$CRED_STORE_OPTS -d"
CRED_STORE_OPTS="$CRED_STORE_OPTS -t x509"


PERL=perl	# Perl program
PERL_PROXY_PATH_FIND='/^path\s+:\s+(\S+)/ && print $1,"\n"'

GRID_PROXY_INFO="grid-proxy-info"
GRID_PROXY_INFO_EXISTS_VALID="0:10"
GRID_PROXY_INFO_EXISTS_OPTS="-valid $GRID_PROXY_INFO_EXISTS_VALID"

GRID_PROXY_INIT=grid-proxy-init

MYPROXYD=myproxy-server
MYPROXY_START_DELAY=4	# daemon startup/shutdown delay
MYPROXY_INIT=myproxy-init
MYPROXY_INIT_OPTS=
MYPROXY_INIT_OPTS="$MYPROXY_INIT_OPTS --verbose"
MYPROXY_INIT_OPTS="$MYPROXY_INIT_OPTS --allow_anonymous_retrievers"
MYPROXY_INIT_OPTS="$MYPROXY_INIT_OPTS --cred_lifetime 1"
MYPROXY_INIT_OPTS="$MYPROXY_INIT_OPTS --proxy_lifetime 1"
MYPROXY_INIT_OPTS="$MYPROXY_INIT_OPTS --stdin_pass"
MYPROXY_INIT_PASS_DELAY=2	# seconds

MYPROXY_INFO=myproxy-info

HOST_VERIFY="ping -c1"
NETSTAT=netstat

CCV=condor_config_val

# Variables
PROG=`basename $0`	# this program name
TEST_COUNT=0	# total count of all tests
FAIL_COUNT=-1	# total count of test failures
X509PROXY=		# "nonstandard" X509 proxy location
GRIDFTPSERVER=localhost
MEM_ERR_FILES=
MEM_LEAK_FILES=
LOG_FILES=
CRED_STORE_DIR=
CRED_NAME=
NEXT_PORT=10000	#next unique TCP server listen port to try
declare -a KILL_PIDS	# array of process PIDs to kill upon exit
declare -a KILL_CMDs	# array of process commands to kill upon exit

# Functions

# Catch signals and program exit.  This function is needed to avoid a bug in
# the bash implmentation of "trap".  The workaround is to always "trap 0"
# before other sigspecs.
sig_handler() {
	# signals to catch.  SIGNAL 0 MUST BE LISTED FIRST!
	local SIGSPEC="0 1 2 15"
	local handler=$1
	local sig
	for sig in $SIGSPEC; do
		trap "$handler" $sig || echo "trap \"$handler\" $sig failed!"
	done
	#trap -p	# for debugging
}

register_log_file () {
	LOG_FILES="$LOG_FILES $1"
}

register_memory_error_file () {
	if $MEMORY_TESTS; then
		MEM_ERR_FILES="$MEM_ERR_FILES $TESTDIR/$1"
	fi
}

register_memory_leak_file () {
	if $LEAK_TESTS; then
		MEM_LEAK_FILES="$MEM_LEAK_FILES $TESTDIR/$1"
	fi
}

suppress_known_leaks () {
	if $LEAK_TESTS; then
		echo leak report supression file $TESTDIR/$VALGRIND_LEAK_SUPPRESS
		# Create a Valgrind leak suppression file for false leaks, or leaks we
		# don't want to hear about.
		cat <<EOF >$VALGRIND_LEAK_SUPPRESS
{
   Alleged SOAP memory leak
   Addrcheck:Leak
   fun:malloc
   fun:soap_register_plugin_arg
   fun:_ZN10DaemonCore14InitHTTPSocketEi
   fun:main
}
EOF
	fi
}

log_files () {
	if [ -z "$LOG_FILES" ]; then
		error no log files have been registered
		return 1
	fi
	name="$1"; shift;
	test_announce "$name"
	outfile="$name.out"
	grep_include=log-include.grep
	cat <<EOF >$grep_include
err
fail
EOF
	grep_exclude=log-exclude.grep
	cat <<EOF >$grep_exclude
module standard error redirected to
EOF
	if $MEMORY_TESTS && [ `uname -s` = "Linux" ] ; then
		# This error occurs when running daemons from under valgrind
		cat <<EOF >>$grep_exclude
getExecPath: readlink("/proc/self/exe") failed: errno 2
EOF
	fi
	grep --ignore-case --with-filename \
		--file=$grep_include $LOG_FILES | \
	grep --ignore-case \
		--invert-match --file=$grep_exclude \
	>$outfile
	status=0
	if [ -s $outfile ]; then
		echo
		cat $outfile
		status=1
	fi
	test_status $status
	return $status
}

memory_errors () {
	if [ -z "$MEM_ERR_FILES" ]; then
		error no memory error files have been registered
		return 1
	fi
	name="$1"; shift;
	test_announce "$name"
	outfile="$name.out"
	grep --with-filename "ERROR SUMMARY:" $MEM_ERR_FILES \
		|  grep -v "ERROR SUMMARY: 0 errors" \
		>$outfile
	status=0
	if [ -s $outfile ]; then
		echo
		cat $outfile
		status=1
	fi
	test_status $status
	return $status
}

memory_leaks () {
	if [ -z "$MEM_LEAK_FILES" ]; then
		error no memory leak files have been registered
		return 1
	fi
	name="$1"; shift;
	test_announce "$name"
	outfile="$name.out"
	grep --with-filename "definitely lost:" $MEM_LEAK_FILES \
		|  grep -v "definitely lost: 0 bytes" \
		>$outfile
	status=0
	if [ -s $outfile ]; then
		echo
		cat $outfile
		status=1
	fi
	test_status $status
	return $status
}

# Announce a test error
error() {
	echo
	echo "  ERROR: " "$@" 1>&2
}

# Register a process id to kill upon program exit
register_kill_pid ()
{
	KILL_PIDS=( "${KILL_PIDS[@]}" "$1" )
}

# Register a command to kill upon program exit
register_kill_cmd ()
{
	KILL_CMDS=( "${KILL_CMDS[@]}" "$1" )
}

# Program cleanup and results summary
finish() {
	# ignore signals until we exit
	#sig_handler	# set a null signal handler so we can finish cleaning up
	#kill 0				# kill any lingering processes in this process group
	# Clean up any lingering background processes.
	local pid
	for pid in ${KILL_PIDS[@]}; do
		kill $pid || error killing PID $pid
	done
	local cmd
	for cmd in "${KILL_CMDS[@]}"; do
		pkill -f "$cmd" || error killing "\"$cmd\""
	done
	wait				# wait for child processes to exit

	echo
	echo scanning logs for errors ...
	if $MEMORY_TESTS; then
		memory_errors "memory_errors"
	fi
	if $LEAK_TESTS; then
		memory_leaks "memory_leaks"
	fi
	log_files "log_files"
	echo
	echo --------------------------------------------------------------
	echo test harness summary: tests: $TEST_COUNT failures: $FAIL_COUNT
	echo results in $TESTDIR
	exit $FAIL_COUNT	# program exit point
}

# Fatal error message and exit
fatal () {
	echo $PROG ERROR: "$@" 1>&2
	exit 
}

# announce a test
test_announce () {
	name=$1; shift;
	printf "test: %-60s -> " $name
}

# update a test status
test_status () {
	status=$1; shift
	(( TEST_COUNT++ ))
	#TEST_COUNT=`expr $TEST_COUNT + 1`
	if [ $status -eq 0 ]; then
		echo pass
	else
		echo FAIL
		FAIL_COUNT=`expr $FAIL_COUNT + 1`
	fi
}

# wait for a file transfer, with timeout
wait_file_cmp() {
	[ $# -lt 2 ] && fatal wait_file_cmp: '<2' args: "$@"
	[ $# -gt 3 ] && fatal wait_file_cmp: '>3' args: "$@"
	new_file=$1; shift;
	match_file=$1; shift;
	timeout=$1; shift;
	[ -z "$timeout" ] && timeout=0	# default timeout=0
	resolution=1

	elapsed=0
	while true; do
		cmp --silent $new_file $match_file && return 0
		(( elapsed++ >= $timeout )) && return 1
		sleep $resolution
	done
}

# wait for a process, timeout
wait_process_name() {
	process_regex="$1"; shift;
	timeout=$1; shift;
	[ -z "$timeout" ] && timeout=0	# default timeout=0
	resolution=1

	elapsed=0
	while true; do
		if ps auwwx | grep -v grep | grep --silent "$process_regex"; then
			#echo pattern \"$process_regex\" found
			return 0
		fi
		(( elapsed++ >= $timeout )) && return 1
		sleep $resolution
	done
}

# Verify X509PROXY
x509proxy() {
	if $X509PROXY_REQD; then
		if $GSI_TESTS; then
			type $GRID_PROXY_INFO >/dev/null \
				|| fatal $GRID_PROXY_INFO command not found.  Check your PATH.
		fi
		if [ -n "$X509PROXY" ]; then
			echo using X509 proxy file $X509PROXY
			proxy_file="-file $X509PROXY"
		else
			echo using X509 proxy file from default search path
			proxy_file=
		fi
		if $GRID_PROXY_INFO $proxy_file -exists $GRID_PROXY_INFO_EXISTS_OPTS;
		then
			echo "proxy valid for at least $GRID_PROXY_INFO_EXISTS_VALID (hh:mm)"
		else
		cat <<EOF 1>&2
$GRID_PROXY_INFO $proxy_file error.  Perhaps proxy has expired,
or will expire within $GRID_PROXY_INFO_EXISTS_VALID (hh:mm).
EOF
			exit 1
		fi
		X509PROXY=`$GRID_PROXY_INFO $proxy_file | $PERL -ne "$PERL_PROXY_PATH_FIND"`
		echo using proxy path \"$X509PROXY\" from $GRID_PROXY_INFO
	fi
}

# test setup
setup_common () {
	[ -c $DEVZERO ] || fatal character special file $DEVZERO does not exist
	[ -c $DEVNULL ] || fatal character special file $DEVNULL does not exist

	if $MEMORY_TESTS; then
		echo memory tests are enabled
		MEMTEST="$VALGRIND $VALGRIND_OPTS"

	else
		echo memory tests are disabled
	fi
	if $LEAK_TESTS; then
		MEMTEST="$MEMTEST $VALGRIND_LEAK_OPTS"
		echo memory leak tests are enabled
	else
		echo memory leak tests are disabled
	fi

	# Basic sanity tests, before beginning
	[ -n "$CONDOR_CONFIG" ] || fatal CONDOR_CONFIG environment not defined
	[ -r $CONDOR_CONFIG ] || fatal $CONDOR_CONFIG file is not readable
	echo CONDOR_CONFIG = $CONDOR_CONFIG
	type $CCV >/dev/null || fatal $CCV binary not found. Check your PATH .
	echo SEC_DEFAULT_AUTHENTICATION = `$CCV SEC_DEFAULT_AUTHENTICATION 2>&1`
	echo SEC_DEFAULT_AUTHENTICATION_METHODS = `$CCV SEC_DEFAULT_AUTHENTICATION_METHODS 2>&1`
	echo SEC_CLIENT_AUTHENTICATION_METHODS = `$CCV SEC_CLIENT_AUTHENTICATION_METHODS 2>&1`

	CRED_NAME=$PROG	# default credential name

	# Update this shell environment to find and run Condor and Globus programs
	BIN=`$CCV BIN`
	SBIN=`$CCV SBIN`
	export PATH="$BIN:$SBIN:$PATH"
	type $STORKD >/dev/null || fatal $STORKD binary not found. Check your PATH.
}

# Create a unique test subdirectory, from current directory. Change to new test
# subdirectory.
cd_test_dir() {
	# create a uniq test directory
	TESTDIR="`pwd`/$PROG-$$"
	mkdir $TESTDIR || fatal mkdir $TESTDIR
	cd $TESTDIR || fatal cd $TESTDIR
	export _CONDOR_LOG=$TESTDIR	# Place Daemoncore logs in TESTDIR
}

# Create a credential subdirectory
cred_dir() {
	# Create a credential subdirectory
	CRED_STORE_DIR=$TESTDIR/credentials
	mkdir $CRED_STORE_DIR || return 1
	export _CONDOR_CRED_STORE_DIR=$CRED_STORE_DIR
	return 0
}

# Return an available TCP port.  Note that this only
# gives an instantaneous snapshot of port usage.
next_port () {
	while (( 1 )) ; do
		port=":$NEXT_PORT +[0-9.]+:"
		if ! $NETSTAT --tcp --all --numeric | egrep --silent "$port" ; then
			break
		fi
		(( NEXT_PORT++ ))
	done
	echo $NEXT_PORT
}

# Startup myproxy server
myproxy_server () {
	export MYPROXY_SERVER_PORT=`next_port`
	echo -n starting $MYPROXYD on port $MYPROXY_SERVER_PORT ...
	type $MYPROXYD >/dev/null \
		|| fatal $MYPROXYD command not found.  Check your PATH.
	local config=$TESTDIR/myproxy-server.config
	local storage=$TESTDIR/myproxy
	mkdir $storage || exit 1
	chmod 0700 $storage || exit 1
	cat <<EOF >$config
accepted_credentials  "*"
authorized_retrievers "*"
default_retrievers    "*"
authorized_renewers   "*"
default_renewers      "none"
EOF

	unset X509_USER_CERT
	unset X509_USER_KEY
	#export X509_USER_PROXY=$X509PROXY	# FIXME: pollutes environment!
	MYPROXYCMD="$MYPROXYD"
	MYPROXYCMD="$MYPROXYCMD --verbose"
	MYPROXYCMD="$MYPROXYCMD -c $config"
	MYPROXYCMD="$MYPROXYCMD --storage $storage"
	MYPROXYCMD="$MYPROXYCMD --port $MYPROXY_SERVER_PORT"
	register_kill_cmd "$MYPROXYCMD"
	local out=$MYPROXYD.out
	(
		export X509_USER_CERT=$X509PROXY
		export X509_USER_KEY=$X509PROXY
		$MYPROXYCMD >$out 2>&1	# start myproxy-server daemon
	)
	export MYPROXY_SERVER_DN=`$GRID_PROXY_INFO -subject -file $X509PROXY`
	export MYPROXY_SERVER_DN=`grid-cert-info -subject`	# FIXME
	sleep $MYPROXY_START_DELAY
	echo
	export MYPROXY_SERVER=localhost
	if ! pgrep -f "$MYPROXYCMD" >$DEVNULL; then
		error start $MYPROXYD failed
		return 1
	fi
	return 0
}

# Load local proxy into myproxy server
store_myproxy () {
	local cred_passphrase="$1"; shift
	local myproxy_pw="$1"; shift
	type $MYPROXY_INIT >/dev/null \
		|| fatal $MYPROXY_INIT command not found.  Check your PATH.
	echo loading $X509PROXY into $MYPROXYD
	out=$MYPROXY_INIT.out
	# Ugh.  After much pain, this is the only known way to specify a pass
	# phrase from stdin.  The VDT install tests use the same method.  Ugh.
	(cat $cred_passphrase; sleep $MYPROXY_INIT_PASS_DELAY; echo $myproxy_pw) | \
		$MYPROXY_INIT $MYPROXY_INIT_OPTS >$out 2>&1
	status=$?
	if [ $status -ne 0 ]; then
		error $MYPROXY_INIT $MYPROXY_INIT_OPTS failed
		cat $out
		return $status
	fi
	out=$MYPROXY_INFO.out
	$MYPROXY_INFO --verbose >$out 2>&1
	return 0
}


# To DO
# MYPROXY_SERVER_DN cannot be a proxy
# use X509_USER_PROXY, X509_USER_KEY, X509_USER_CERT?  see globus security config

# Store a credential
store_cred() {
	name="$1"; shift;
	test_announce "$name"
	#CRED_STORE_OPTS="$CRED_STORE_OPTS -n $CREDD_HOST"
	cmd="$CRED_STORE $CRED_STORE_OPTS -f $X509PROXY -N $CRED_NAME"
	run_fg $name $cmd
	status=$?
	if [ $status -ne 0 ];then
		test_status $status
		error $cmd failed
		return $status
	fi
#	ncredentials=`count_dir_credentials`
#	if [ $ncredentials -ne 1 ]; then
#		status=1
#		test_status $status
#		error $CRED_STORE_DIR credential count: $ncredentials, should be 1
#		return $status
#	fi
	wait_file_cmp $X509PROXY $CRED_STORE_DIR/cred[^-]* 0
	status=$?
	if [ $status -ne 0 ];then
		test_status $status
		error X509 proxy not copied to $CRED_STORE_DIR
		return $status
	fi
	test_status $status
	return $status
}

# Run program from memory tester in the background
run_bg() {
	out="$1"; shift;
	if $MEMORY_TESTS; then
		LOGFILE="--log-file=$out"
	fi
	$MEMTEST $LOGFILE "$@" >"$out.out" 2>&1 &
	status=$?	# is this pointless?
	register_kill_pid $!
	return $status
}

# Run program from memory tester in the foreground
run_fg() {
	out="$1"; shift;
	if $MEMORY_TESTS; then
		LOGFILE="--log-file=$out"
	fi
	$MEMTEST $LOGFILE "$@" >"$out.out" 2>&1
	status=$?
	return $status
}

# start credd daemon
credd () {
	name="$1"; shift;

	# Update config via environment
	export _CONDOR_CRED_CHECK_INTERVAL=2
	export _CONDOR_CREDD_ADDRESS_FILE="$TESTDIR/.credd_address"
	CREDD_PORT=`next_port`
	#CREDD_HOST_PORT="${CREDD_HOST}:${CREDD_PORT}"
	CREDD_OPTS="$CREDD_OPTS -p $CREDD_PORT"
	cmd="$CREDD $CREDD_OPTS"
	echo starting $cmd ...
	test_announce "$name"
	# FIXME: create unique command port with next_port
	run_bg $name $cmd
	CREDD_PID="$!"
	register_memory_error_file "$name.pid*"
	register_memory_leak_file "$name.pid*"
	register_log_file "CredLog"
	sleep $CREDD_START_DELAY
	ps >/dev/null $CREDD_PID
	status=$?
	test_status $status
	if [ $status -ne 0 ];then
		error $cmd failed
	fi
	return $status
}

