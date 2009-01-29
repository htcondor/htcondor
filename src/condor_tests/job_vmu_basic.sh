#!/bin/sh

echo "IT WORKED" > /tmp/condorout
dd if=/tmp/condorout of=/dev/fd0
