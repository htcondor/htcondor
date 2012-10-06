#!/bin/sh

if [ $# -ne 2 ]; then
  echo "Usage: ./nat_delete_script.sh JOBID DEV"
  exit 1
fi

JOBID=$1
DEV=$2
PUBLIC_DEV="em1"

iptables -D FORWARD -i $DEV -o $PUBLIC_DEV -g $JOBID
iptables -D FORWARD -o $DEV -i $PUBLIC_DEV -g $JOBID -m state --state RELATED,ESTABLISHED
iptables -t nat -D POSTROUTING -o $PUBLIC_DEV -j MASQUERADE

iptables -D $JOBID 1
rc=$?
while [ $rc -eq 0 ]; do
  iptables -D $JOBID 1 2> /dev/null
  rc=$?
done

iptables -X $JOBID

