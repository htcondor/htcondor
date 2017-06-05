#!/bin/sh
#
# The message generated here is unlikely to be visible, but it shows how to
# determine if HTCondor is exiting voluntarily (because it ran out of work)
# or not (the Spot instance is being terminated or the machine is otherwise
# being shut down).
#
SHUTDOWN=`/sbin/runlevel | /usr/bin/awk '{print $2}'`
SHUTDOWN_MESSAGE='because instance is being terminated'
if [ ${SHUTDOWN} -ne 0 ]; then
    SHUTDOWN_MESSAGE='for lack of work'
fi
INSTANCE_ID=$(/usr/bin/curl -s http://169.254.169.254/latest/meta-data/instance-id)
MESSAGE="$(/bin/date) Instance ${INSTANCE_ID} shutting down ${SHUTDOWN_MESSAGE}."
echo ${MESSAGE}

/sbin/shutdown -h now
exit 0
