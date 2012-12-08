#!/bin/sh

if [ $# -ne 3 ]; then
  echo "Usage: ./nat_delete_script.sh JOBID DEV INNER_IP"
  exit 1
fi

JOBID=$1
DEV=$2
JOB_INNER_IP=$3

iptables -D FORWARD -i $DEV ! -o $DEV -g $JOBID 2> /dev/null
iptables -D FORWARD -o $DEV ! -i $DEV -g $JOBID -m state --state RELATED,ESTABLISHED 2> /dev/null
iptables -t nat -D POSTROUTING --src $JOB_INNER_IP ! --dst $JOB_INNER_IP -j MASQUERADE

DELETE_DIR=`dirname $0`
$DELETE_DIR/lark_cleanup_firewall $JOBID

#iptables -X $JOBID

