/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"

#include "classadlogentry.h"
#include "prober.h"
#include "classadlogparser.h"

//! constructor
Prober::Prober()
{
	jqfile_last_mod_time = 0;
	jqfile_last_size = 0;

}

//! destructor
Prober::~Prober()
{}

//**************************************************************************
// Accessors
//**************************************************************************
void
Prober::setJobQueueName(char* jqn)
{
	if(!jqn) {
		EXCEPT("Expecting jqn to be not null here\n");
	}
	strcpy(job_queue_name, jqn);
}

char*
Prober::getJobQueueName()
{
	return job_queue_name;
}

void
Prober::setJQFile_Last_MTime(time_t t)
{
	jqfile_last_mod_time = t;
}


time_t
Prober::getJQFile_Last_MTime()
{
	return jqfile_last_mod_time;
}

void
Prober::setJQFile_Last_Size(size_t s)
{
	jqfile_last_size = s;
}


size_t
Prober::getJQFile_Last_Size()
{
	return jqfile_last_size;
}


//! probe job_queue.log file
ProbeResultType
Prober::probe(ClassAdLogEntry *curCALogEntry,char const *job_queue_name)
{
	FileOpErrCode   st;
	int op_type;

	struct stat fstat;
	//TODO: should use condor's StatInfo instead.
	if (stat(job_queue_name, &fstat) == -1)
		dprintf(D_ALWAYS,"ERROR: calling stat()\n");
	
	dprintf(D_ALWAYS, "=== Current Probing Information ===\n");
	dprintf(D_ALWAYS, "fsize: %ld\t\tmtime: %ld\n", 
				(long)fstat.st_size, (long)fstat.st_mtime);

	// get the new state
	cur_probed_jqfile_last_mod_time = fstat.st_mtime;
	cur_probed_jqfile_last_size = fstat.st_size;

	if (jqfile_last_mod_time == 0) {
		// This is the start phase of quill
		return INIT_QUILL;
	}	
		// If there is no change in last modification time,
		// no action is needed.
	if (fstat.st_mtime >= jqfile_last_mod_time) {

		if (fstat.st_size > jqfile_last_size) {
				// File has been increased
				// New logs may be added
				// ------- ACTION ----------
				// Get the Delta & Check Last Command
				// 1. if it's the same as the last one, that's normal addition
				// return ADDITION;
				// 2. if not, it's error state
				// return PROBE_ERROR;

				//
				// check the last command
			ClassAdLogParser caLogParser;

			caLogParser.setJobQueueName((char *)job_queue_name);


			caLogParser.setNextOffset(curCALogEntry->offset);
			st = caLogParser.readLogEntry(op_type);

			if (st != FILE_READ_SUCCESS) {
				return PROBE_ERROR;
			}

			if (caLogParser.getCurCALogEntry()->equal(curCALogEntry))
			{ 
				return ADDITION;
			}
			else 
			{
				return PROBE_ERROR;
			}
		}
		else if (fstat.st_size < jqfile_last_size) {
				// File has been decreased
				// Compression may happen
				return COMPRESSED;
		}
		else {
				// File has not been increased or decreased
				// ------- ACTION ----------
				// Need to check last command --> 2 cases
				// 1. if it's the same as the last one, that's OK
				// return NO_CHANGE; 
				// 2. if not, it's error state
				// return PROBE_ERROR;

				//
				// check the last command

			ClassAdLogParser caLogParser;
			caLogParser.setJobQueueName((char *)job_queue_name);

			caLogParser.setNextOffset(curCALogEntry->offset);

			st = caLogParser.readLogEntry(op_type); 

			if (st != FILE_READ_EOF && st != FILE_READ_SUCCESS) {
				return PROBE_ERROR;
			}

			if (caLogParser.getCurCALogEntry()->equal(curCALogEntry)) { 
				return NO_CHANGE;
			}
			else {
				return PROBE_ERROR;
			}
		}
	}
	else {
		return PROBE_ERROR;
	}

	return NO_CHANGE;
}

void
Prober::incrementProbeInfo() {
	// store the currently probed stat
	jqfile_last_mod_time = cur_probed_jqfile_last_mod_time;
	jqfile_last_size = cur_probed_jqfile_last_size;
}
