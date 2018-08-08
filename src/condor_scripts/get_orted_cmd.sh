#!/bin/sh

##**************************************************************
##
## Copyright (C) 1990-2018, Condor Team, Computer Sciences Department,
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

# This script gets launched in place of ssh when mpirun is invoked in
# openmpiscript. It captures the command that mpirun is expecting to run
# on each execute node and chirps the command back to the spool directory,
# where it can be fetched by the execute nodes.

# Get the node from the command-line
NODE=$1
shift

# Chirp the orted command to spool
CONDOR_CHIRP=$(condor_config_val libexec)/condor_chirp
echo "$@" | $CONDOR_CHIRP put -mode cwa -perm 400 - $_CONDOR_REMOTE_SPOOL_DIR/orted_cmd.$NODE

