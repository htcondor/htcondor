#!/bin/bash
# This is a resource agent for controlling the condor_schedd from 
# cluster suite.  It is a modified version of the condor init script
# developed by Matt Farrellee.

# The program being managed
prog=condor_schedd

pidfile=/var/run/condor/$prog-$OCF_RESKEY_name.pid

# Source function library
. /etc/init.d/functions
. $(dirname $0)/ocf-shellfuncs

# Source networking configuration
[ -f /etc/sysconfig/network ] && . /etc/sysconfig/network

# Source Condor configuration
[ -f /etc/sysconfig/condor ] && . /etc/sysconfig/condor

# Check that networking is up
[ "${NETWORKING}" = "no" ] && exit 1


metadata() {
  cat <<EOT
<?xml version="1.0"?>
<!DOCTYPE resource-agent SYSTEM "ra-api-1-modified.dtd">
<resource-agent version="rgmanager 2.0" name="condor_schedd">
  <version>1.0</version>

  <longdesc lang="en">
    condor_schedd resource agent
  </longdesc>
  <shortdesc lang="en">
    condor_schedd resource agent
  </shortdesc>

  <parameters>
    <parameter name="name" unique="1" primary="1">
    <longdesc lang="en">
      The name of the condor_schedd instance to control
    </longdesc>
    <shortdesc lang="en">
    condor_schedd name
    </shortdesc>
    <content type="string"/>
    </parameter>
  </parameters>

  <actions>
    <action name="start" timeout="5"/>
    <action name="stop" timeout="5"/>
    <action name="recover" timeout="5"/>
    <action name="monitor" interval="30" timeout="5"/>
    <action name="status" interval="30" timeout="5"/>
    <action name="meta-data" timeout="5"/>
  </actions>
</resource-agent>
EOT
}

start() {
    verify_binary
    if [ $? -ne 0 ]; then
      return $?
    fi

    ocf_log info "Starting condor_schedd $OCF_RESKEY_name"
    pid_status $pidfile
    if [ $? -ne 0 ]; then
        rm -f $pidfile
    fi
    condor_config_val $OCF_RESKEY_name > /dev/null 2> /dev/null
    if [ $? -ne 0 ]; then
      # Configuration for schedd isn't on this node, so fail to start
      ocf_log err "condor_schedd $OCF_RESKEY_name not configured on this node"
      return $OCF_ERR_GENERIC
    fi
    daemon --pidfile $pidfile --check $prog $prog -pidfile $pidfile -local-name $OCF_RESKEY_name
    RETVAL=$?
    echo
    if [ $RETVAL -ne 0 ]; then
        ocf_log err "Failed to start condor_schedd $OCF_RESKEY_name"
        return $OCF_ERR_GENERIC
    fi
    return 0
}

stop() {
    verify_binary
    if [ $? -ne 0 ]; then
      return $?
    fi

    ocf_log info "Stopping condor_schedd $OCF_RESKEY_name"
    killproc -p $pidfile $prog -QUIT
    RETVAL=$?
    echo
    # Shutdown can take a long time
    wait_pid $pidfile 600
    if [ $? -ne 0 ]; then
        ocf_log err "Failed to stop condor_schedd $OCF_RESKEY_name"
	RETVAL=$OCF_ERR_GENERIC
    fi
    return $RETVAL
}

#
# Determine if a process is running only by looking in a pidfile.
# There is no use of pidof, which can find processes that are not
# started by this script.
#
# ASSUMPTION: The pidfile will exist if the process does, see false
# negative warning.
#
# WARNING: A false positive is possible if the process that dropped
# the pid file has crashed and the pid has been recycled. A false
# negative is possible if the process has not yet dropped the pidfile,
# or it contains the incorrect pid.
#
# Usage: pid_status <pidfile>
# Result: 0 = pid exists
#         1 = pid does not exist, but pidfile does
#         2 = pidfile does not exist, thus pid does not exist
#         3 = status unknown
#
pid_status() {
    pid=$(get_pid $1)
    case $? in
	1) return 2 ;;
	2) return 3 ;;
    esac

    ps $pid &>/dev/null
    if [ $? -ne 0 ]; then

	return 1
    fi

    return 0
}

#
# Wait for the pid in the pidfile to disappear, but only do so for at
# most timeout seconds.
#
# Usage: wait_pid <pidfile> <timeout>
# Result: 0 = pid was not found (doesn't exist or not accessible)
#         1 = pid still exists after timeout
wait_pid() {
    pid=$(get_pid $1)
    if [ $? -ne 0 ]; then
	return 0
    fi

    wait=0
    while [ $wait -lt $2 ]; do
	pid_status $1
	if [ $? -ne 0 ]; then
	    return 0
        fi

	sleep 1
	wait=$((wait + 1))
    done

    return 1
}

#
# Retrieve pid from a pidfile
#
# Usage: get_pid <pidfile>
# Result: 0 = pid returned
#         1 = pidfile not found
#         2 = pidfile not accessible or didn't contain pid
# Stdout: pid
#
get_pid() {
    if [ -f $1 ]; then
	pid=`cat $1` &>/dev/null
	if [ $? -ne 0 -o -z "$pid" ]; then
	    return 2
	fi

	echo -n $pid
	return 0
    fi

    return 1
}

#
# Verify the binary is installed
#
# Usage: verify_binary
# Result: $OCF_ERR_INSTALLED = binary not installed
#         0                  = binary installed
#
verify_binary() {
    # Report that $prog does not exist, or is not executable
    if [ ! -x /usr/sbin/$prog ]; then
	return $OCF_ERR_INSTALLED
    fi

    return 0
}


case "$1" in
    start)
	pid_status $pidfile 
	[ $? -eq 0 ] && return 0
	start
	exit $?
	;;

    stop)
	if ! stop; then
	    exit $OCF_ERR_GENERIC
	fi
	exit 0
	;;

    status|monitor)
	pid_status $pidfile 
	if [ $? -ne 0 ]; then
	    exit $OCF_NOT_RUNNING
	fi

	# WARNING: status uses pidof and may find more pids than it
	# should.
	status -p $pidfile $prog
	exit $?
	;;

    meta-data)
	metadata
	exit 0
	;;

    recover|restart)
	$0 stop || exit $OCF_ERR_GENERIC
	$0 start || exit $OCF_ERR_GENERIC
	exit 0
	;;

    validate-all)
	[ -n "$OCF_RESKEY_name" ] || exit $OCF_ERR_ARGS

	# xxx do nothing for now.
	exit 0
	;;
    *)
	exit $OCF_ERR_UNIMPLEMENTED
esac
