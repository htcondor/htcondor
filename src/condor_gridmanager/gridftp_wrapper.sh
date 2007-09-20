#!/bin/sh
unset GLOBUS_LOCATION
GRIDMAP=`pwd`/$GRIDMAP
export GRIDMAP
GSI_AUTHZ_CONF=/doesnotexist
export GSI_AUTHZ_CONF
cmd=$1
shift
exec $cmd -c /dev/null "$@"
