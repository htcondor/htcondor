#!/bin/bash
# This is a resource agent for controlling condor daemons from 
# cluster.  It is based on the condor init script developed
# by Matt Farrellee, but modified to be a Resource Agent and to support
# multiple daemons.

if [ "$OCF_RESKEY_type" = "query_server" ]; then
  prefix="aviary"
else
  prefix="condor"
fi

# The program being managed
prog=${prefix}_$OCF_RESKEY_type

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
<resource-agent version="rgmanager 2.0" name="condor">
  <version>1.0</version>

  <longdesc lang="en">
    condor resource agent
  </longdesc>
  <shortdesc lang="en">
    condor resource agent
  </shortdesc>

  <parameters>
    <parameter name="name" unique="1" primary="1">
    <longdesc lang="en">
      The name passed to the condor subsystem type
    </longdesc>
    <shortdesc lang="en">
      condor subsystem name
    </shortdesc>
    <content type="string"/>
    </parameter>

    <parameter name="type" required="1">
    <longdesc lang="en">
      The type of condor subsystem
    </longdesc>
    <shortdesc lang="en">
      condor subsystem type
    </shortdesc>
    <content type="string"/>
    </parameter>
  </parameters>

  <actions>
    <action name="start" timeout="15"/>
    <action name="stop" timeout="605"/>
    <action name="recover" timeout="630"/>
    <action name="monitor" interval="30" timeout="5"/>
    <action name="status" interval="30" timeout="5"/>
    <action name="meta-data" timeout="5"/>
    <action name="validate-all" timeout="5"/>
  </actions>
</resource-agent>
EOT
}

start() {
  verify_binary
  RETVAL=$?
  if [ $RETVAL -ne 0 ]; then
    return $RETVAL
  fi

  ocf_log info "Starting $prog $OCF_RESKEY_name"
  pid_status $pidfile
  if [ $? -ne 0 ]; then
    rm -f $pidfile
  fi

  # Verify the schedd is configured on this node
  condor_config_val $OCF_RESKEY_name > /dev/null 2> /dev/null
  if [ $? -ne 0 ]; then
    # Configuration for schedd isn't on this node, so fail to start
    ocf_log err "$prog $OCF_RESKEY_name not configured on this node"
    return $OCF_ERR_GENERIC
  fi

  # Setup environment variables so the schedd can use it's own procd
  env=`condor_config_val ${OCF_RESKEY_name}_ENVIRONMENT`
  if [ $? -eq 0 ]; then
    name=`echo $env | cut -d'=' -f 1`
    val=`echo $env | cut -d'=' -f 2-`
    export $name=$val
  fi

  # Daemonize the schedd process
  daemon --pidfile $pidfile --check $prog $prog -pidfile $pidfile -local-name $OCF_RESKEY_name
  RETVAL=$?
  unset $name
  echo
  if [ $RETVAL -ne 0 ]; then
    ocf_log err "Failed to daemonize $prog $OCF_RESKEY_name"
    return $OCF_ERR_GENERIC
  fi

  # Verify the process started
  wait=0
  while [ $wait -lt 10 ]; do
    pid_status $pidfile
    if [ $? -eq 0 ]; then
      return 0
    fi

    sleep 1
    wait=$((wait + 1))
  done
  pid_status $pidfile
  if [ $? -ne 0 ]; then
    ocf_log err "Failed to start $prog $OCF_RESKEY_name"
    rm -f $OCF_ERR_GENERIC
  fi
  return 0
}

stop() {
  ocf_log info "Stopping $prog $OCF_RESKEY_name"
  killproc -p $pidfile $prog -QUIT
  RETVAL=$?
  echo
  # Shutdown can take a long time
  wait_pid $pidfile 600
  if [ $? -ne 0 ]; then
    ocf_log err "Failed to stop $prog $OCF_RESKEY_name"
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
    ocf_log err "Binary /usr/sbin/$prog doesn't exist"
    return $OCF_ERR_INSTALLED
  fi

  return 0
}


case "$1" in
  start)
    pid_status $pidfile 
    [ $? -eq 0 ] && exit 0
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

    exit 0
    ;;
  *)
    exit $OCF_ERR_UNIMPLEMENTED
esac
