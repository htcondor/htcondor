#!/bin/sh

if [ $# -ne 2 ]; then
  echo "Usage: nat_script.sh <JOB ID> <outer dev>"
  exit 1
fi

ON_CAMPUS="129.93.0.0/16"
PUBLIC_DEV=em1

JOB_OUTER_PREFIX="192.168.0.1/24"
JOB_INNER_IP="192.168.0.2"

JOBID=$1
DEV=$2

# Minimal configuration of iptables rules in the system chains
iptables -t nat -A POSTROUTING -o $PUBLIC_DEV -j MASQUERADE || exit 2
iptables -N $JOBID || exit 2
iptables -A FORWARD -i $DEV -o $PUBLIC_DEV -g $JOBID || exit 2
iptables -A FORWARD -i $PUBLIC_DEV -o $DEV -m state --state RELATED,ESTABLISHED -g $JOBID || exit 2

# Outgoing packets
iptables -A $JOBID -i $DEV -o $PUBLIC_DEV --dst $ON_CAMPUS -j ACCEPT -m comment --comment "OutgoingInternal" || exit 3
iptables -A $JOBID -i $DEV -o $PUBLIC_DEV ! --dst $ON_CAMPUS -j ACCEPT -m comment --comment "OutgoingExternal" || exit 3

# Incoming established TCP connections
iptables -A $JOBID -i $PUBLIC_DEV -o $DEV -m state --state RELATED,ESTABLISHED --src $ON_CAMPUS -j ACCEPT -m comment --comment "IncomingInternal" || exit 4
iptables -A $JOBID -i $PUBLIC_DEV -o $DEV -m state --state RELATED,ESTABLISHED ! --src $ON_CAMPUS -j ACCEPT -m comment --comment "IncomingExternal" || exit 4

# Drop everything else
iptables -A $JOBID -j REJECT || exit 5

ifconfig $DEV $JOB_OUTER_PREFIX up || exit 6

echo $JOB_INNER_IP

