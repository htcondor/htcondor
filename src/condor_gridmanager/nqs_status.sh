#!/bin/bash

blahconffile="${GLITE_LOCATION:-/}/etc/blah.config"
binpath=`grep nqs_binpath $blahconffile|grep -v \#|awk -F"=" '{ print $2}'|sed -e 's/ //g'|sed -e 's/\"//g'`

###############################################################
# Parse parameters
###############################################################

while getopts "wn" arg 
do
    case "$arg" in
    w) getwn="yes" ;;
    n) getcreamport="yes" ;;
    
    -) break ;;
    ?) echo $usage_string
       exit 1 ;;
    esac
done

shift `expr $OPTIND - 1`

# TODO Check for failure (non-zero exit code, qstat doesn't exist, etc)
requested=`echo $1 | sed 's/^.*\///'`
requestedshort=`expr match "$requested" '\([0-9]*\)'`
# We're looking for a line like one of these:
#    1:    .nqs_submit     20106.gridhpc          condor1  61  RUNNING     17831
#    1:.nqs_submit.185     20107.gridhpc          condor1  61  RUNNING     18594
#result=`${binpath}/qstat | awk -v jobid="$requestedshort" '
#$2 ~ jobid {
#	print $5
#}
#$3 ~ jobid {
#	print $6
#}
#'`
result=`${binpath}/qstat | grep " ${requestedshort}[.]" | sed -e 's/.*'${requestedshort}'//' | awk '{ print $4 }'`
if [ "$result" == "" ] ; then
	# Job completed, get exit status
	# TODO get real exit code
	echo '0[JobStatus=4;ExitCode=0;]'
elif [ "$result" == "RUNNING" -o "$result" == 'RUNNING*' -o "$result" == "EXITING" ] ; then
	echo '0[JobStatus=2;]'
else
	echo '0[JobStatus=1;]'
fi
