/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef CONDOR_SUBMIT_H
#define CONDOR_SUBMIT_H

#include "condor_id.h"

/** Submits a job to condor using popen().  This is a very primitive method
    to submitting a job, and SHOULD be replacable by a Condor Submit API.

    In the mean time, this function executes the condor_submit command
    via popen() and parses the output, sniffing for the CondorID assigned
    to the submitted job.

    Parsing the condor_submit output successfully depends on the current
    version of Condor, and how it's condor_submit outputs results on the
    command line.
   
	@param dm the appropriate Dagman object
	@param cmdFile the job's Condor command file.
	@param condorID will hold the ID for the submitted job (if successful)
	@param DAGNodeName the name of the job's DAG node
	@param DAGParentNodeNames a delimited string listing the node's parents
	@param vars list of any variables for this node
	@param retry the retry number (0 the first time the job is run)
	@param directory the directory in which to run this job
	@param default log file name
	@param whether to use the default log
	@param log file to force this job to use (should be null if submit
		file specifies log file)
	@param prohibitMultiJobs flag that prohibits multiple jobs in a
		cluster from being submitted.
	@param hold_claim is true if DAGMAN_HOLD_CLAIM_IDLE is positive
	@return true on success, false on failure
*/

bool condor_submit( const Dagman &dm, const char* cmdFile, CondorID& condorID,
					const char* DAGNodeName, MyString &DAGParentNodeNames,
					List<Job::NodeVar> *vars, int retry,
					const char* directory, const char *defLog, bool useDefLog,
					const char *logFile, bool prohibitMultiJobs,
					bool hold_claim );

bool stork_submit( const Dagman &dm, const char* cmdFile, CondorID& condorID,
				   const char* DAGNodeName, const char* directory );

void set_fake_condorID( int subprocID );

bool fake_condor_submit( CondorID& condorID, Job* job, const char* DAGNodeName,
					const char* directory, const char *logFile,
					bool logIsXml );

int get_fake_condorID();

bool writePreSkipEvent( CondorID& condorID, Job* job, const char* DAGNodeName, 
			   const char* directory, const char *logFile, bool logIsXml );

#endif /* #ifndef CONDOR_SUBMIT_H */
