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

#ifndef _PROBER_H_
#define _PROBER_H_

#include "condor_common.h"
#include "quill_enums.h"

//! Prober
/*! this polls and probes Job Qeueue Log (job_queue.log) file.
 *  So, it returns the result of polling: INIT_DB, ADDITION, 
 *  COMPRESSION, and so on.
 */
class Prober
{
public:
	//! constructor	
	Prober();
	//! destructor	
	~Prober();
	
	//! initialization
	void			Init();

	//! probe job_queue.log file
	ProbeResultType probe(ClassAdLogEntry *curCALogEntry,
						  int job_queue_fd);

	//! update state information about size of log file last probed etc.
	//! Call this after successfully responding to probe() result.
	void incrementProbeInfo();

		//
		// accessors
		//
	void			setJobQueueName(const char* jqn);
	char*			getJobQueueName();
	void			setLastModifiedTime(time_t t);
	time_t			getLastModifiedTime();
	void			setLastSize(size_t s);
	size_t			getLastSize();

	long int		getLastSequenceNumber();
	void			setLastSequenceNumber(long int seq_num);
	time_t			getLastCreationTime();
	void			setLastCreationTime(time_t ctime);

	long int		getCurProbedSequenceNumber();
	long int 		getCurProbedCreationTime();

private:
		// information about a job_queue.log file and polling state
	char			job_queue_name[_POSIX_PATH_MAX]; //!< job queue file path
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

#endif /* _PROBER_H_ */
