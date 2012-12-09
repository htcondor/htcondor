#!/bin/sh

# lark_cleanup_script.sh -
# The cleanup script is run after the job has run and Lark has done its own
# cleanup routines.
#
# This provides sites with a mechanism to cleanup after their custom startup
# scripts.
#
# Note that this script doesn't do anything specific to cleanup after the
# sample script. That is because Lark automatically removes the iptables
# chain associated with the job.
#

if [ $# -ne 3 ]; then
  echo "Usage: ./nat_delete_script.sh JOBID DEV INNER_IP"
  exit 1
fi

JOBID=$1
DEV=$2
JOB_INNER_IP=$3

exit 0
