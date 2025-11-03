#!/bin/bash

#  File:     pbs_status.sh
#
#  Author:   David Rebatto
#  e-mail:   David.Rebatto@mi.infn.it
#
#
#  Revision history:
#    20-Mar-2004: Original release
#    04-Jan-2005: Totally rewritten, qstat command not used anymore
#    03-May-2005: Added support for Blah Log Parser daemon (using the pbs_BLParser flag)
#
#  Description:
#    Return a classad describing the status of a PBS job
#
# Copyright (c) Members of the EGEE Collaboration. 2004. 
# See http://www.eu-egee.org/partners/ for details on the copyright
# holders.  
# 
# Licensed under the Apache License, Version 2.0 (the "License"); 
# you may not use this file except in compliance with the License. 
# You may obtain a copy of the License at 
# 
#     http://www.apache.org/licenses/LICENSE-2.0 
# 
# Unless required by applicable law or agreed to in writing, software 
# distributed under the License is distributed on an "AS IS" BASIS, 
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
# See the License for the specific language governing permissions and 
# limitations under the License.
#

. `dirname $0`/blah_load_config.sh

if [ -x ${blah_libexec_directory}/pbs_status.py ] ; then
    exec ${blah_libexec_directory}/pbs_status.py "$@"
fi

if [ "x$job_registry" != "x" ] ; then
   ${blah_sbin_directory}/blah_job_registry_lkup $@
   exit 0
fi

usage_string="Usage: $0 [-w] [-n]"

logpath=${pbs_spoolpath}/server_logs
if [ ! -d $logpath -o ! -x $logpath ]; then
 pbs_spoolpath=`${pbs_binpath}/tracejob | grep 'default prefix path'|awk -F" " '{ print $5 }'`
 logpath=${pbs_spoolpath}/server_logs
fi

#get worker node info
getwn=""

#get creamport
getcreamport=""

usedBLParser="no"

srvfound=""

BLClient="${blah_libexec_directory}/BLClient"

qstatuser=`whoami`
qstatcache=/tmp/qstatcache_${qstatuser}.txt
qstattmp=`mktemp -q /tmp/qstattmp_XXXXXXXXXX`
`rm -f $qstattmp`

############################
#functions
############################

function search_wn(){

	workernode=""
	if [ -e $qstatcache ] ; then
		workernode=`grep $reqjob $qstatcache|awk -F":" '{ print $2 }'`
	fi
	if [ "x$workernode" == "x" ] ; then
		result=`${pbs_binpath}/qstat -f 2> /dev/null | awk -v cqstat=$qstattmp '
BEGIN {
    current_job = ""
    current_wn = ""
}

/Job Id:/ {
    current_job = substr($0, index($0, ":") + 2)
    end = index(current_job, ".")
    if ( end == 0 ) { end = length(current_job) + 1 }
    current_job = substr(current_job, 1, end)
}
/exec_host =/ {
    current_wn = substr($0, index($0, "=")+2)
    current_wn = substr(current_wn, 1, index(current_wn, "/")-1)
    print current_job":"current_wn>cqstat
}
'
`
		`mv -f $qstattmp $qstatcache`
		
		if [ -e $qstatcache ] ; then
			workernode=`grep $reqjob $qstatcache|awk -F":" '{ print $2 }'`
		fi
	fi

}

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

if [ "x$pbs_nologaccess" == "xyes" ]; then

#Try different logparser
 if [ ! -z $pbs_num_BLParser ] ; then
  for i in `seq 1 $pbs_num_BLParser` ; do
   s=`echo pbs_BLPserver${i}`
   p=`echo pbs_BLPport${i}`
   eval tsrv=\$$s
   eval tport=\$$p
   testres=`echo "TEST/"|$BLClient -a $tsrv -p $tport`
   if [ "x$testres" == "xYPBS" ] ; then
    pbs_BLPserver=$tsrv
    pbs_BLPport=$tport
    srvfound=1
    break
   fi
  done
  if [ -z $srvfound ] ; then
   echo "1ERROR: not able to talk with no logparser listed"
   exit 0
  fi
 fi
fi

###################################################################
#get creamport and exit

if [ "x$getcreamport" == "xyes" ] ; then
 result=`echo "CREAMPORT/"|$BLClient -a $pbs_BLPserver -p $pbs_BLPport`
 reqretcode=$?
 if [ "$reqretcode" == "1" ] ; then
  exit 1
 fi
 retcode=0
 echo $pbs_BLPserver:$result
 exit $retcode
fi

pars=$*

for  reqfull in $pars ; do
	requested=""
	#header elimination
	requested=${reqfull:4}
	reqjob=`echo $requested | sed -e 's/^.*\///'`
	logfile=`echo $requested | sed 's/\/.*//'`
	
     if [ "x$pbs_nologaccess" == "xyes" ]; then

        staterr=/tmp/${reqjob}_staterr
	
result=`${pbs_binpath}/qstat -f $reqjob 2>$staterr`
qstat_exit_code=$?
result=`echo "$result" | awk -v jobId=$reqjob '
BEGIN {
    current_job = ""
    current_wn = ""
    current_js = ""
    exitcode = "-1"
}

/Job Id:/ {
    current_job = substr($0, index($0, ":") + 2)
    end = index(current_job, ".")
    if ( end == 0 ) { end = length(current_job) + 1 }
    current_job = substr(current_job, 1, end)
    print "[BatchJobId=\"" current_job "\";"
}
/exec_host =/ {
    current_wn = substr($0, index($0, "=")+2)
    current_wn = substr(current_wn, 1, index(current_wn, "/")-1)
}

/job_state =/ {
    current_js = substr($0, index($0, "=")+1)
}

/exit_status =/ {
    exitcode = substr($0, index($0, "=")+1)
}

END {
        if (current_js ~ "Q")  {jobstatus = 1}
        if (current_js ~ "W")  {jobstatus = 1}
        if (current_js ~ "S")  {jobstatus = 1}
        if (current_js ~ "T")  {jobstatus = 1}
        if (current_js ~ "R")  {jobstatus = 2}
        if (current_js ~ "E")  {jobstatus = 2}
        if (current_js ~ "C")  {jobstatus = 4}
        if (current_js ~ "H")  {jobstatus = 5}
	if (exitcode ~ "271")  {jobstatus = 3}
	
	if (jobstatus == 2 || jobstatus == 4) {
		print "WorkerNode=\"" current_wn "\";"
	}
	print "JobStatus=" jobstatus ";"
	if (jobstatus == 4) {
		if (exitcode == "-1") {
			exitcode = "0"
		}
		print "ExitCode=" exitcode ";"
	}
	print "]"

}
'
`
        errout=`cat $staterr`
	rm -f $staterr 2>/dev/null
	
        if [ -z "$errout" ] ; then
                echo "0"$result
                retcode=0
        elif [ "$qstat_exit_code" -eq "153" ] ; then
                # If the job has disappeared, assume it's completed 
                # (same as globus)
                echo "0[BatchJobId=\"$reqjob\";JobStatus=4;ExitCode=0]"
                retcode=0
        else
                echo "1ERROR: Job not found"
                retcode=1
        fi

     else

	cliretcode=0
	retcode=0
	logs=""
	result=""
	logfile=`echo $requested | sed 's/\/.*//'`
	if [ "x$pbs_BLParser" == "xyes" ] ; then
    		usedBLParser="yes"
		result=`echo $requested | $BLClient -a $pbs_BLPserver -p $pbs_BLPport`
		cliretcode=$?
		response=${result:0:1}
		if [ "$response" != "[" -o "$cliretcode" != "0" ] ; then
			cliretcode=1
		else 
			cliretcode=0
		fi
	fi
	if [ "$cliretcode" == "1" -a "x$pbs_fallback" == "xno" ] ; then
	 echo "1ERROR: not able to talk with logparser on ${pbs_BLPserver}:${pbs_BLPport}"
	 exit 0
	fi
	if [ "$cliretcode" == "1" -o "x$pbs_BLParser" != "xyes" ] ; then
		result=""
		usedBLParser="no"
		logs="$logpath/$logfile `find $logpath -type f -newer $logpath/$logfile`"
		log_data=`grep "$reqjob" $logs`
		result=`echo "$log_data" | awk -v jobId="$reqjob" -v wn="$workernode" '
BEGIN {
	rex_queued   = jobId ";Job Queued "
	rex_running  = jobId ";Job Run "
	rex_deleted  = jobId ";Job deleted "
	rex_finished = jobId ";Exit_status="
	rex_hold     = jobId ";Holds .* set"
	rex_released = jobId ";Holds .* released"
	saved_jobstatus = -1

	print "["
	print "BatchjobId = \"" jobId "\";"
}

$0 ~ rex_queued {
	jobstatus = 1
}

$0 ~ rex_running {
	jobstatus = 2
}

$0 ~ rex_deleted {
	jobstatus = 3
	exit
}

$0 ~ rex_finished {
	jobstatus = 4
	s = substr($0, index($0, "Exit_status="))
	s = substr(s, 1, index(s, " ")-1)
	exitcode = substr(s, index(s, "=")+1)
	exit
}

$0 ~ rex_hold {
	saved_jobstatus = jobstatus
	jobstatus = 5
}

$0 ~ rex_released {
	if (saved_jobstatus > 0) {
		jobstatus = saved_jobstatus
	}
	saved_jobstatus = -1
}

END {
	if (jobstatus == 0) { exit 1 }
	print "JobStatus = " jobstatus ";"
	if (jobstatus == 2) {
		print "WorkerNode = \"" wn "\";"
	}
	if (jobstatus == 4) {
		print "ExitCode = " exitcode ";"
	}
	print "]"
}
' `

  		if [ "$?" == "0" ] ; then
			echo "0"$result
			retcode=0
  		else
			echo "1ERROR: Job not found"
			retcode=1
  		fi
	fi #close if on pbs_BLParser
        if [ "x$usedBLParser" == "xyes" ] ; then
                result=`echo $result | sed 's/\/.*//'`

		resstatus=`echo $result|sed "s/\[.*JobStatus=\([^;]*\).*/\1/"`;
                if [ "$resstatus" != "1" -a "x$getwn" == "xyes" ] ; then
                        search_wn
                fi

                res=`echo $result|sed "s/\[.*ExitCode=\([^;]*\).*/\1/"`;
                if [ "$res" == "271" ] ; then
                        out=`sed -n 's/^=>> PBS: //p' *.e$reqjob 2>/dev/null`
                        if [ ! -z $out ] ; then
                                echo "0"$result "Workernode=\"$workernode\"; ExitReason=\"$out\";]"
                        else
                                echo "0"$result "Workernode=\"$workernode\"; ExitReason=\"Killed by Resource Management System\";]"
                        fi
                else
                        echo "0"$result "Workernode=\"$workernode\";]"
                fi

                usedBLParser="no"
        fi
     fi #close of if-else on $pbs_nologaccess
done 
exit 0
