#!/bin/sh

ifconfig eth0 | awk -F '[:[:blank:]]+' '/inet addr:/{print $4}' > /tmp/condorout
dd if=/tmp/condorout of=/dev/fd0
while [ ! -f /tmp/file_from_ssh ]
do
	sleep 1
done
cp /tmp/file_from_ssh /tmp/condorout
dd if=/tmp/condorout of=/dev/fd0
