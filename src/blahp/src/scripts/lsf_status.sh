#!/bin/bash

#  File:     lsf_status.sh
#
#  Author:   David Rebatto, Massimo Mezzadri
#  e-mail:   David.Rebatto@mi.infn.it, Massimo.Mezzadri@mi.infn.it
#
#
#  Revision history:
#    20-Mar-2004: Original release
#    22-Feb-2005: Totally rewritten, bhist command not used anymore
#     3-May-2005: Added support for Blah Log Parser daemon (using the lsf_BLParser flag)
#
#  Description:
#    Return a classad describing the status of a LSF job
#
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

if [ -x ${blah_bin_directory}/lsf_status.py ] ; then
    exec ${blah_bin_directory}/lsf_status.py "$@"
fi

if [ "x$job_registry" != "x" ] ; then
   ${blah_sbin_directory}/blah_job_registry_lkup $@
   exit 0
fi

conffile=$lsf_confpath/lsf.conf
lsf_confdir=`cat $conffile|grep LSF_CONFDIR| awk -F"=" '{ print $2 }'`
[ -f ${lsf_confdir}/profile.lsf ] && . ${lsf_confdir}/profile.lsf

usage_string="Usage: $0 [-w] [-n]"

#get worker node info (dummy for LSF)
getwn=""

#get creamport
getcreamport=""

usedBLParser="no"
   
srvfound=""

BLClient="${blah_libexec_directory}/BLClient"

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

if [ "x$lsf_nologaccess" == "xyes" ]; then

#Try different log parser
 if [ ! -z $lsf_num_BLParser ] ; then
  for i in `seq 1 $lsf_num_BLParser` ; do
   s=`echo lsf_BLPserver${i}`
   p=`echo lsf_BLPport${i}`
   eval tsrv=\$$s
   eval tport=\$$p
   testres=`echo "TEST/"|$BLClient -a $tsrv -p $tport`
   if [ "x$testres" == "xYLSF" ] ; then
    lsf_BLPserver=$tsrv
    lsf_BLPport=$tport
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
 result=`echo "CREAMPORT/"|$BLClient -a $lsf_BLPserver -p $lsf_BLPport`
 reqretcode=$?
 if [ "$reqretcode" == "1" ] ; then
  exit 1
 fi
 retcode=0
 echo $lsf_BLPserver:$result
 exit $retcode
fi

pars=$*

for  reqfull in $pars ; do
     reqfull=${reqfull:4}
     requested=`echo $reqfull | sed -e 's/^.*\///'`
     datenow=`echo $reqfull | sed 's/\/.*//'`
		     
     if [ "x$lsf_nologaccess" == "xyes" ]; then

        staterr=/tmp/${requested}_staterr

result=`${lsf_binpath}/bhist -l $requested 2>/dev/null | awk -v jobId=$requested '
BEGIN {
    current_job = ""
    current_wn = ""
    jobstatus = 0
}

/Job </ {
    current_job = substr($0, index($0, "<")+1)
    current_job = substr(current_job, 1, index(current_job, ">") -1)
    
    print "[BatchJobId=\"" current_job "\";"
}

/Dispatched to/ {
    current_wn = substr($0, index($0, "<")+1)
    current_wn = substr(current_wn, 1, index(current_wn, ">") -1)
}

/Submitted from/ { jobstatus = 1 }

/Starting / { jobstatus = 2 }

/Signal <KILL>/ { 
                 jobstatus = 3 
		 exit
		 }

/Done successfully/ { 
                    jobstatus = 4
		    exitcode = 0
		    exit
		    }

/Exited with exit code/ { 
                         jobstatus = 4
		         exitcode = substr($0, index($0, ".")-4) 
		         exitcode = substr(exitcode, index(exitcode, ".")-1, index(exitcode, " ")-2)
			 exit 
		        }
/Suspended/ { suspended = 1 }

/resumed/ { suspended = 0 }


END {        
	if (jobstatus == 0) { exit 1 }
	if (suspended == 1) {jobstatus=5} 
	if (jobstatus == 2 || jobstatus == 4) {
		print "WorkerNode=\"" current_wn "\";"
	}
	print "JobStatus=" jobstatus ";"
	if (jobstatus == 4) {
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
        else
                echo "1ERROR: Job not found"
                retcode=1
        fi


     else
		
	result=""
	cliretcode=0
	if [ "x$lsf_BLParser" == "xyes" ] ; then
    
		usedBLParser="yes"
		result=`echo $reqfull| $BLClient -a $lsf_BLPserver -p $lsf_BLPport`
    		cliretcode=$?
                response=${result:0:1}
		if [ "$response" != "[" -o "$cliretcode" != "0" ] ; then
                        cliretcode=1
                else
                        cliretcode=0
                fi
	fi
	
        if [ "$cliretcode" == "1" -a "x$lsf_fallback" == "xno" ] ; then
         echo "1ERROR: not able to talk with logparser on ${lsf_BLPserver}:${lsf_BLPport}"
         exit 0
        fi

	if [ "$cliretcode" == "1" -o "x$lsf_BLParser" != "xyes" ] ; then
		result=""
		usedBLParser="no"
		datefile=/tmp/blahdate_$RANDOM$RANDOM$RANDOM
		touch $datefile;chmod 600 $datefile

		if [ $? -ne 0 ]; then
   			echo '1ERROR: Could not create temporary file'
   			datefile=""
			echo "1ERROR: Job not found"
			break
		fi

		conffile=$lsf_confpath/lsf.conf
		lsf_base_path=`cat $conffile|grep LSB_SHAREDIR| awk -F"=" '{ print $2 }'`
		lsf_clustername=`${lsf_binpath}/lsid | grep 'My cluster name is'|awk -F" " '{ print $5 }'`
		logpath=$lsf_base_path/$lsf_clustername/logdir
		logeventfile=lsb.events
		touch -t ${datenow}0000 $datefile
		ulogs=`find $logpath -name $logeventfile.[0-9]* -maxdepth 1 -type f -newer $datefile -print 2>/dev/null`
		rm -f $datefile 2>/dev/null
		for i in `echo $ulogs | sed "s|${logpath}/${logeventfile}\.||g" | sort -nr`; do
 			logs="$logs$logpath/$logeventfile.$i "
		done
		logs="$logs$logpath/$logeventfile"

#/* job states */
#define JOB_STAT_NULL         0x00
#define JOB_STAT_PEND         0x01
#define JOB_STAT_PSUSP        0x02
#define JOB_STAT_RUN          0x04
#define JOB_STAT_SSUSP        0x08
#define JOB_STAT_USUSP        0x10
#define JOB_STAT_EXIT         0x20
#define JOB_STAT_DONE         0x40
#define JOB_STAT_PDONE        (0x80)  /* Post job process done successfully */
#define JOB_STAT_PERR         (0x100) /* Post job process has error */
#define JOB_STAT_WAIT         (0x200) /* Chunk job waiting its turn to exec */
#define JOB_STAT_UNKWN        0x10000

job_data=`grep "$requested" $logs`

result=`echo "$job_data" | awk -v jobId=$requested '
BEGIN {
	rex_queued   = "\"JOB_NEW\" \"[0-9\.]+\" [0-9]+ " jobId
	rex_running  = "\"JOB_START\" \"[0-9\.]+\" [0-9]+ " jobId
	rex_deleted  = "\"JOB_SIGNAL\" \"[0-9\.]+\" [0-9]+ " jobId " [0-9]+ [0-9]+ \"KILL\""
	rex_done     = "\"JOB_STATUS\" \"[0-9\.]+\" [0-9]+ " jobId " 192 "
	rex_pperr    = "\"JOB_STATUS\" \"[0-9\.]+\" [0-9]+ " jobId " 320 "
	rex_finished = "\"JOB_STATUS\" \"[0-9\.]+\" [0-9]+ " jobId " 32 "
	rex_phold    = "\"JOB_STATUS\" \"[0-9\.]+\" [0-9]+ " jobId " 2 "
        rex_shold    = "\"JOB_STATUS\" \"[0-9\.]+\" [0-9]+ " jobId " 8 "
 	rex_uhold    = "\"JOB_STATUS\" \"[0-9\.]+\" [0-9]+ " jobId " 16 "
 	rex_pend     = "\"JOB_STATUS\" \"[0-9\.]+\" [0-9]+ " jobId " 1 "
	jobstatus = 0
	
	print "["
	print "BatchjobId = \"" jobId "\";"
}

$0 ~ rex_queued {
	jobstatus = 1
}

$0 ~ rex_pend {
	jobstatus = 1
}

$0 ~ rex_running {
	jobstatus = 2
        print "WorkerNode = " $10 ";"
}

$0 ~ rex_deleted {
	jobstatus = 3
	exit
}

$0 ~ rex_done {
	jobstatus = 4
	exitcode = 0
	exit
}

$0 ~ rex_pperr {
	jobstatus = 4
	exitcode = -1
	exit
}

$0 ~ rex_finished {
	jobstatus = 4
	exitcode = $(NF-2)
	exit
}

$0 ~ rex_uhold {
	jobstatus = 7
}

$0 ~ rex_phold {
	jobstatus = 1
}

$0 ~ rex_shold {
	jobstatus = 7
}

END {
	if (jobstatus == 0) { exit 1 }
	print "JobStatus = " jobstatus ";"
	if (jobstatus == 4) {
		print "ExitCode = " exitcode ";"
		if (exitcode == 130) {
			print "ExitReason = \" Memory limit reached \";"
		}else if(exitcode == 137){
			print "ExitReason = \" Memory limit reached \";"
		}else if(exitcode == 140){
			print "ExitReason = \" RUNtime limit reached \";"
		}else if(exitcode == 143){
			print "ExitReason = \" Memory limit reached \";"
		}else if(exitcode == 152){
			print "ExitReason = \" CPUtime limit reached \";"
		}else if(exitcode == 153){
			print "ExitReason = \" FILEsize limit reached \";"
		}else if(exitcode == 157){
			print "ExitReason = \" Directory Access Error (No AFS token, dir does not exist) \";"
		}
	}
	print "]"
}
' `

   		if [ "$?" == "0" ] ; then
        		echo "0"$result
   		else
        		echo "1ERROR: Job not found"
   		fi
	fi #close if on BLParser

	if [ "x$usedBLParser" == "xyes" ] ; then

		result=`echo $result | sed 's/\/.*//'`
    		echo "0"$result
		usedBLParser="no"	
	fi
	logs=""
	
     fi #close of if-else on $lsf_nologaccess
done
exit 0

