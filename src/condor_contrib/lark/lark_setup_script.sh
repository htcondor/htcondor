#!/bin/sh

#
# lark_setup_script.sh -
# This script is meant to be provided or customized by the sysadmin.
# It is executed after the system-level portion of the network is
# configured and before the job is executed.
#
# This allows for site-level network customization for Condor jobs.
#

if [ $# -ne 4 ]; then
  echo "Usage: lark_setup_script.sh <internal IPv4 address> <external IPv4 address> <starter iptables chain> <outer dev>"
  exit 1
fi

ON_CAMPUS="129.93.0.0/16"

JOB_OUTER_IP="$1"
JOB_INNER_IP="$2"

JOBID="$3"
DEV="$4"

# For this example, we add additional network counters to the job.
# This separately accounts for packets from on-campus versus off-campus

# Outgoing packets
iptables -A $JOBID -i $DEV ! -o $DEV --dst $ON_CAMPUS -j ACCEPT -m comment --comment "OutgoingInternal" || exit 3
iptables -A $JOBID -i $DEV ! -o $DEV ! --dst $ON_CAMPUS -j ACCEPT -m comment --comment "OutgoingExternal" || exit 3

# Incoming established TCP connections
iptables -A $JOBID ! -i $DEV -o $DEV -m state --state RELATED,ESTABLISHED --src $ON_CAMPUS -j ACCEPT -m comment --comment "IncomingInternal" || exit 4
iptables -A $JOBID ! -i $DEV -o $DEV -m state --state RELATED,ESTABLISHED ! --src $ON_CAMPUS -j ACCEPT -m comment --comment "IncomingExternal" || exit 4

# Drop everything else
iptables -A $JOBID -j REJECT || exit 5

