#!/bin/sh

if [ $# -ne 4 ]; then
  echo "Usage: nat_script.sh <internal IPv4 address> <external IPv4 address> <JOB ID> <outer dev>"
  exit 1
fi

ON_CAMPUS="129.93.0.0/16"
#PUBLIC_DEV=em1

JOB_OUTER_IP=$1
JOB_OUTER_PREFIX="$JOB_OUTER_IP/24"
JOB_INNER_IP=192.168.181.7
JOB_INNER_IP=$2

JOBID="$3"
DEV="$4"

# Minimal configuration of iptables rules in the system chains
#iptables -t nat -A POSTROUTING -o $PUBLIC_DEV -j MASQUERADE || exit 2
iptables -t nat -A POSTROUTING --src $JOB_INNER_IP ! --dst $JOB_INNER_IP -j MASQUERADE || exit 2
iptables -N $JOBID || exit 2
iptables -I FORWARD -i $DEV ! -o $DEV -g $JOBID || exit 2
iptables -I FORWARD ! -i $DEV -o $DEV -m state --state RELATED,ESTABLISHED -g $JOBID || exit 2

# Outgoing packets
iptables -A $JOBID -i $DEV ! -o $DEV --dst $ON_CAMPUS -j ACCEPT -m comment --comment "OutgoingInternal" || exit 3
iptables -A $JOBID -i $DEV ! -o $DEV ! --dst $ON_CAMPUS -j ACCEPT -m comment --comment "OutgoingExternal" || exit 3

# Incoming established TCP connections
iptables -A $JOBID ! -i $DEV -o $DEV -m state --state RELATED,ESTABLISHED --src $ON_CAMPUS -j ACCEPT -m comment --comment "IncomingInternal" || exit 4
iptables -A $JOBID ! -i $DEV -o $DEV -m state --state RELATED,ESTABLISHED ! --src $ON_CAMPUS -j ACCEPT -m comment --comment "IncomingExternal" || exit 4

# Drop everything else
iptables -A $JOBID -j REJECT || exit 5

#ifconfig $DEV $JOB_OUTER_PREFIX up || exit 6
ip addr add $JOB_OUTER_IP/255.255.255.255 dev $DEV || exit 6
ip link set dev $DEV up || exit 7
route add $JOB_INNER_IP/32 gw $JOB_OUTER_IP || exit 8

