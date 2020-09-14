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

#ifndef _CLASSADLOGPROBER_H_
#define _CLASSADLOGPROBER_H_

#ifdef _NO_CONDOR_
#include <time.h> // for time_t
#include <unistd.h> // for size_t
#include <stdio.h> // for FILE*
#else
#include "condor_common.h"
#endif

enum ProbeResultType {  PROBE_ERROR, 
						PROBE_FATAL_ERROR,
						NO_CHANGE, 
						ADDITION, 
						COMPRESSED};

//! ClassAdLogProber
/*! this polls and probes Job Qeueue Log (job_queue.log) file.
 *  So, it returns the result of polling: INIT_DB, ADDITION, 
 *  COMPRESSION, and so on.
 */
class ClassAdLogProber
{
public:
	//! constructor	
	ClassAdLogProber();
	//! destructor	
	~ClassAdLogProber();
	
	//! initialization
	void			Init();

	//! probe job_queue.log file
	ProbeResultType probe(ClassAdLogEntry *curCALogEntry,
						  FILE* job_queue_fp);

	//! update state information about size of log file last probed etc.
	//! Call this after successfully responding to probe() result.
	void incrementProbeInfo();

		//
		// accessors
		//
	void			setJobQueueName(const char* jqn);
	char*			getJobQueueName();
	void			setLastModifiedTime(time_t t);
	time_t			getLastModifiedTime() const;
	void			setLastSize(size_t s);
	size_t			getLastSize() const;

	long int		getLastSequenceNumber() const;
	void			setLastSequenceNumber(long int seq_num);
	time_t			getLastCreationTime() const;
	void			setLastCreationTime(time_t ctime);

	long int		getCurProbedSequenceNumber() const;
	long int 		getCurProbedCreationTime() const;

private:
		// information about a job_queue.log file and polling state
	char			job_queue_name[PATH_MAX]; //!< job queue file path
		// stored metadata in DB
	long int		last_mod_time;	//!< last modification time
	long int		last_size;	//!< last size
	long int		last_seq_num;	//!< historical sequence number
	long int		last_creation_time; //!< creation time of cur file 
		// currently probed...
	long int		cur_probed_mod_time; //!< last modification time
	long int		cur_probed_size;	 //!< last size
	long int		cur_probed_seq_num;	//!< historical sequence number
	long int		cur_probed_creation_time;  //!< creation time of cur file 

	ClassAdLogEntry	lastCALogEntry;		//!< last command (ClassAd Log Entry)

};

#endif /* _CLASSADLOGPROBER_H_ */
