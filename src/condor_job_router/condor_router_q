#!/bin/sh
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

PrintUsage() {
  echo "USAGE: $0 [router_q options] [condor_q options]"
  echo
  echo "This is a convenience wrapper around condor_q for viewing"
  echo "jobs being managed by the condor job router."
  echo
  echo "router_q options:"
  echo "  -S             summarize state of jobs on each route"
  echo "  -R             summarize running jobs on each route"
  echo "  -I             summarize idle jobs on each route"
  echo "  -H             summarize held jobs on each route"
  echo "  -route <name>  show only jobs on specified route"
  echo "  -idle          show only idle jobs"
  echo "  -held          show only held jobs"
  echo "  -constraint X  show jobs matching specified constraint"
  echo
  echo "Usage for condor_q follows:"
  condor_q -h
}

CONSTRAINT='RouteName =!= UNDEFINED'
show_route_summary=0
while true; do
  if [ "$1" = "-S" ]; then
    show_route_summary=1
    shift
  elif [ "$1" = "-R" ]; then
    show_route_summary=1
    CONSTRAINT="$CONSTRAINT && JobStatus == 2"
    shift
  elif [ "$1" = "-I" ]; then
    show_route_summary=1
    CONSTRAINT="$CONSTRAINT && JobStatus != 2"
    shift
  elif [ "$1" = "-H" ]; then
    show_route_summary=1
    CONSTRAINT="$CONSTRAINT && JobStatus == 5"
    shift
  elif [ "$1" = "-idle" ]; then
    CONSTRAINT="$CONSTRAINT && JobStatus == 1"
    shift
  elif [ "$1" = "-held" ]; then
    CONSTRAINT="$CONSTRAINT && JobStatus == 5"
    shift
  elif [ "$1" = "-constraint" ]; then
    CONSTRAINT="$CONSTRAINT && ($2)"
    shift
    shift
  elif [ "$1" = "-route" ] || [ "$1" = "-site" ]; then
    shift
    CONSTRAINT="$CONSTRAINT && RouteName =?= \"$1\""
    shift
  elif [ "$1" = "-h" ] || [ "$1" = "-help" ]; then
    PrintUsage
    exit 0
  else
    break
  fi
done

SECOND_FLAG=
if condor_config_val JOB_ROUTER_SCHEDD2_NAME 2>&1 > /dev/null; then
	SECOND_SCHEDD=`condor_config_val JOB_ROUTER_SCHEDD2_NAME 2> /dev/null`
	SECOND_FLAG="-name ${SECOND_SCHEDD} "
fi

if condor_config_val JOB_ROUTER_SCHEDD2_POOL 2>&1 > /dev/null; then
	SECOND_POOL=`condor_config_val JOB_ROUTER_SCHEDD2_POOL 2> /dev/null`
	SECOND_FLAG="$SECOND_FLAG -pool ${SECOND_POOL}"
fi

if [ "$show_route_summary" = "1" ]; then
  awk 'BEGIN {printf("%7s %2s %-20s %s\n","JOBS","ST","Route","GridResource")}'
  condor_q ${SECOND_FLAG} -constraint "$CONSTRAINT" -f "%s," JobStatus -f "%s," RouteName -f "%s" GridResource -f "\n" eol |
    awk -F, '{
      jobstatus=$1;
      if(jobstatus == 0) jobstatus = "U";
      if(jobstatus == 1) jobstatus = "I";
      if(jobstatus == 2) jobstatus = "R";
      if(jobstatus == 3) jobstatus = "X";
      if(jobstatus == 4) jobstatus = "C";
      if(jobstatus == 5) jobstatus = "H";
      if(jobstatus == 6) jobstatus = "E";
      printf("%2s %-20s %s\n",jobstatus,$2,$3);
    }' | sort | uniq -c | sort -k 3
  exit 0
fi

condor_q -constraint "$CONSTRAINT" "$@"
