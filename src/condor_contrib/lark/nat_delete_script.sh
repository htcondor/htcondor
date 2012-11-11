#!/bin/sh

if [ $# -ne 3 ]; then
  echo "Usage: ./nat_delete_script.sh JOBID DEV INNER_IP"
  exit 1
fi

JOBID=$1
DEV=$2
JOB_INNER_IP=$3

iptables -D FORWARD -i $DEV ! -o $DEV -g $JOBID
iptables -D FORWARD -o $DEV ! -i $DEV -g $JOBID -m state --state RELATED,ESTABLISHED
#iptables -t nat -D POSTROUTING -o $PUBLIC_DEV -j MASQUERADE
iptables -t nat -D POSTROUTING --src $JOB_INNER_IP ! --dst $JOB_INNER_IP -j MASQUERADE

#iptables -D $JOBID 1
#rc=$?
#while [ $rc -eq 0 ]; do
#  iptables -D $JOBID 1 2> /dev/null
#  rc=$?
#done

/home/bbockelm/projects/condor/src/condor_contrib/lark/lark_cleanup_firewall $JOBID

#iptables -X $JOBID

