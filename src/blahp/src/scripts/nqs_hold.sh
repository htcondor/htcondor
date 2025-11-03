#!/bin/bash

blahconffile="${GLITE_LOCATION:-/}/etc/blah.config"
binpath=`grep nqs_binpath $blahconffile|grep -v \#|awk -F"=" '{ print $2}'|sed -e 's/ //g'|sed -e 's/\"//g'`

requested=`echo $1 | sed 's/^.*\///'`
requestedshort=`expr match "$requested" '\([0-9]*\)'`
result=`${binpath}/qstat | awk -v jobid="$requestedshort" '
$2 ~ jobid {
	print $5
}
'`
#currently only holding idle or waiting jobs is supported
if [ "$2" ==  "1" ] ; then
	${binpath}/qhold $requested
else
	if [ "$result" == "W" ] ; then
		${binpath}/qhold $requested
	else
		exit 1
	fi
fi
