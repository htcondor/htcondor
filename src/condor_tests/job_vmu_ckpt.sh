#!/bin/sh

touch /tmp/marker
echo STARTED > /tmp/condorout
dd if=/tmp/condorout of=/dev/fd0
echo CHECKPOINTED > /tmp/expected
while true
do
	dd if=/dev/fd0 of=/tmp/status bs=`stat -c %s /tmp/expected` count=1
	if cmp /tmp/status /tmp/expected
	then
		break
	fi
	sleep 1
done
if [ -f /tmp/marker ]
then
	echo DONE > /tmp/condorout
else
	echo FAILED > /tmp/condorout
fi
dd if=/tmp/condorout of=/dev/fd0
