#!/bin/sh
GRIDMAP=`pwd`/$GRIDMAP
export GRIDMAP
exec ${1+"$@"}
