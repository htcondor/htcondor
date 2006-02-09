#!/bin/sh
GRIDMAP=`pwd`/$GRIDMAP
export GRIDMAP
cmd=$1
shift
exec $cmd -c /dev/null "$@"
