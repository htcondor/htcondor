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
  echo "USAGE: $0 [router_rm options] [condor_rm options]"
  echo
  echo "This is a convenience wrapper around condor_rm for removing"
  echo "jobs being managed by the condor job router."
  echo
  echo "router_rm options:"
  echo "  -route <name>  remove only jobs on specified route"
  echo "  -idle          remove only idle jobs"
  echo "  -held          remove only held jobs"
  echo "  -constraint X  remove jobs matching specified constraint"
  echo
  echo "NOTE: If you wish to remove based on job.id or username simply"
  echo "execute:"
  echo "   condor_rm job.id, or condor_rm username"
  echo 
  echo "Usage for condor_rm follows:"
  condor_rm -h
}

CONSTRAINT='RouteName =!= UNDEFINED'
while true; do
  if [ "$1" = "-idle" ]; then
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

  break
done

condor_rm "$@" -constraint "$CONSTRAINT" 
