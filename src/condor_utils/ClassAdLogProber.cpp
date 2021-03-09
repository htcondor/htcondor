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

#ifdef _NO_CONDOR_
#include <sys/types.h> // for fstat, struct stat
#include <sys/stat.h> // for fstat, struct stat
#include <unistd.h> // for fstat, struct stat
#include <limits.h> // for _POSIX_PATH_MAX
#include <string.h> // for strcpy
#include <stdlib.h> // for atol
#include <assert.h> // for assert
#include <errno.h> // for errno
#include <syslog.h> // for syslog, LOG_ERR, LOG_DEBUG
#else
#include "condor_common.h"
#endif

#include "ClassAdLogEntry.h"
#include "ClassAdLogProber.h"
#include "ClassAdLogParser.h"

//! constructor
ClassAdLogProber::ClassAdLogProber()
{
	last_mod_time = 0;
	last_size = 0;
	last_seq_num = 0;
	last_creation_time = 0;
	
	cur_probed_mod_time = 0;
	cur_probed_size = 0;
	cur_probed_seq_num = 0;
	cur_probed_creation_time = 0;
	job_queue_name[0] = '\0';
}

//! destructor
ClassAdLogProber::~ClassAdLogProber()
{}

//**************************************************************************
// Accessors
//**************************************************************************
void
ClassAdLogProber::setJobQueueName(const char* jqn)
{
	assert(jqn);
	strncpy(job_queue_name, jqn, PATH_MAX - 1);
	job_queue_name[PATH_MAX - 1] = '\0';
}

char*
ClassAdLogProber::getJobQueueName()
{
	return job_queue_name;
}

void
ClassAdLogProber::setLastModifiedTime(time_t t)
{
	last_mod_time = t;
}


time_t
ClassAdLogProber::getLastModifiedTime() const
{
	return last_mod_time;
}

void
ClassAdLogProber::setLastSize(size_t s)
{
	last_size = s;
}


size_t
ClassAdLogProber::getLastSize() const
{
	return last_size;
}

long int
ClassAdLogProber::getLastSequenceNumber() const 
{
	return last_seq_num;
}

void
ClassAdLogProber::setLastSequenceNumber(long int seq_num) 
{
	last_seq_num = seq_num;
}

time_t
ClassAdLogProber::getLastCreationTime() const 
{
	return last_creation_time;
}

void
ClassAdLogProber::setLastCreationTime(time_t ctime) 
{
	last_creation_time = ctime;
}

long int
ClassAdLogProber::getCurProbedSequenceNumber() const {
	return cur_probed_seq_num;
}

long int
ClassAdLogProber::getCurProbedCreationTime() const {
	return cur_probed_creation_time;
}



//! probe job_queue.log file
ProbeResultType
ClassAdLogProber::probe(ClassAdLogEntry *curCALogEntry,
			  FILE * job_queue_fp)
{
	FileOpErrCode   st;
	int op_type = -1;
	struct stat filestat;
	int job_queue_fd = fileno(job_queue_fp);
	//TODO: uncomment and possibly change
	/* 
	if (job_queue_fd == -1) {
		return PROBE_ERROR;
	}
	*/

	//TODO: should use condor's StatInfo instead.
	if (fstat(job_queue_fd, &filestat) == -1)
#ifdef _NO_CONDOR_
		syslog(LOG_ERR, "ERROR: calling stat(): errno=%d (%m)", errno);
	
	syslog(LOG_DEBUG, "=== Current Probing Information ===");
	syslog(LOG_DEBUG,
		   "fsize: %ld\t\tmtime: %ld", 
		   (long)filestat.st_size, (long)filestat.st_mtime);
#else
		dprintf(D_ALWAYS,"ERROR: calling stat() on %p - %s (errno=%d)\n", job_queue_fp, strerror(errno), errno);
	
	dprintf(D_FULLDEBUG, "=== Current Probing Information ===\n");
	dprintf(D_FULLDEBUG, "fsize: %ld\t\tmtime: %ld\n", 
				(long)filestat.st_size, (long)filestat.st_mtime);
#endif

	// get the new state
	cur_probed_mod_time = filestat.st_mtime;
	cur_probed_size = filestat.st_size;

	ClassAdLogParser caLogParser;
	
	caLogParser.setFilePointer(job_queue_fp);
	caLogParser.setNextOffset(0);
	st = caLogParser.readLogEntry(op_type);

	if (FILE_FATAL_ERROR == st) {
		return PROBE_FATAL_ERROR;
	}
	if (st != FILE_READ_SUCCESS) {
		return PROBE_ERROR;
	}

	if ( caLogParser.getCurCALogEntry()->op_type !=
		 CondorLogOp_LogHistoricalSequenceNumber )
	{
#ifdef _NO_CONDOR_
		syslog(LOG_ERR,
			   "ERROR: prober expects first classad log entry to be "
			   "type %d, but sees %d instead.",
			   CondorLogOp_LogHistoricalSequenceNumber,
			   caLogParser.getCurCALogEntry()->op_type);
#endif
		return PROBE_FATAL_ERROR;
	}

#ifdef _NO_CONDOR_
	syslog(LOG_DEBUG,
		   "first log entry: %s %s %s", 
		   ((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->key,
		   ((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->name,
		   ((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->value);
#else
	dprintf(D_FULLDEBUG, "first log entry: %s %s %s\n", 
			((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->key,
			((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->name,
			((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->value);
#endif
	cur_probed_seq_num = 
		atol(((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->key);
	cur_probed_creation_time = 
		atol(((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->value);

	if(cur_probed_seq_num != last_seq_num) {
		return COMPRESSED;
	}

	caLogParser.setNextOffset(curCALogEntry->offset);
	st = caLogParser.readLogEntry(op_type);
	
	if (FILE_FATAL_ERROR == st) {
		return PROBE_FATAL_ERROR;
	}
	if (st != FILE_READ_EOF && st != FILE_READ_SUCCESS) {
		return PROBE_ERROR;
	}
		

	if((filestat.st_size == last_size) && 
	   caLogParser.getCurCALogEntry()->equal(curCALogEntry)) {
			// File size and contents stay the same
		return NO_CHANGE;
	}
	else if((filestat.st_size > last_size) &&
			caLogParser.getCurCALogEntry()->equal(curCALogEntry)) {
			// File has been increased and new entries have been added
		return ADDITION;
	}
		//if it wasn't captured by any of the above cases, 
		//it must have been an error
	return PROBE_ERROR;		
}

void
ClassAdLogProber::incrementProbeInfo() {
	// store the currently probed stat
	last_mod_time = cur_probed_mod_time;
	last_size = cur_probed_size;

	last_seq_num = cur_probed_seq_num;
	last_creation_time = cur_probed_creation_time;
}
