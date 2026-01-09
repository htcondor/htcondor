#!/bin/bash

. `dirname $0`/blah_load_config.sh

requested=`echo $1 | sed 's/^.*\///'`
requestedshort=`expr match "$requested" '\([0-9]*\)'`
result=`${nqs_binpath}qstat | awk -v jobid="$requestedshort" '
$2 ~ jobid {
	print $5
}
'`
#currently only holding idle or waiting jobs is supported
if [ "$2" ==  "1" ] ; then
	${nqs_binpath}qhold $requested
else
	if [ "$result" == "W" ] ; then
		${nqs_binpath}qhold $requested
	else
		exit 1
	fi
fi
