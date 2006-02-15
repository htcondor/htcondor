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
	last_mod_time = 0;
	last_size = 0;
	last_seq_num = 0;
	last_creation_time = 0;
	
	cur_probed_mod_time = 0;
	cur_probed_size = 0;
	cur_probed_seq_num = 0;
	cur_probed_creation_time = 0;
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
Prober::setLastModifiedTime(time_t t)
{
	last_mod_time = t;
}


time_t
Prober::getLastModifiedTime()
{
	return last_mod_time;
}

void
Prober::setLastSize(size_t s)
{
	last_size = s;
}


size_t
Prober::getLastSize()
{
	return last_size;
}

long int
Prober::getLastSequenceNumber() 
{
	return last_seq_num;
}

void
Prober::setLastSequenceNumber(long int seq_num) 
{
	last_seq_num = seq_num;
}

time_t
Prober::getLastCreationTime() 
{
	return last_creation_time;
}

void
Prober::setLastCreationTime(time_t ctime) 
{
	last_creation_time = ctime;
}

long int
Prober::getCurProbedSequenceNumber() {
	return cur_probed_seq_num;
}

long int
Prober::getCurProbedCreationTime() {
	return cur_probed_creation_time;
}



//! probe job_queue.log file
ProbeResultType
Prober::probe(ClassAdLogEntry *curCALogEntry,
			  int job_queue_fd)
{
	FileOpErrCode   st;
	int op_type;
	struct stat filestat;

	//TODO: should use condor's StatInfo instead.
	if (fstat(job_queue_fd, &filestat) == -1)
		dprintf(D_ALWAYS,"ERROR: calling stat()\n");
	
	dprintf(D_ALWAYS, "=== Current Probing Information ===\n");
	dprintf(D_ALWAYS, "fsize: %ld\t\tmtime: %ld\n", 
				(long)filestat.st_size, (long)filestat.st_mtime);

	// get the new state
	cur_probed_mod_time = filestat.st_mtime;
	cur_probed_size = filestat.st_size;

	ClassAdLogParser caLogParser;
	
	caLogParser.setFileDescriptor(job_queue_fd);
	caLogParser.setNextOffset(0);
	st = caLogParser.readLogEntry(op_type);
	
	if (st != FILE_READ_SUCCESS) {
		return ERROR;
	}

	dprintf(D_ALWAYS, "first log entry: %s %s %s\n", 
			((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->key,
			((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->name,
			((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->value);
	cur_probed_seq_num = 
		atol(((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->key);
	cur_probed_creation_time = 
		atol(((ClassAdLogEntry *)caLogParser.getCurCALogEntry())->value);

	if (last_size == 0) {
			// starting phase
		return INIT_QUILL;
	}	

	if(cur_probed_seq_num != last_seq_num) {
		return COMPRESSED;
	}

	caLogParser.setNextOffset(curCALogEntry->offset);
	st = caLogParser.readLogEntry(op_type);
	
	if (st != FILE_READ_EOF && st != FILE_READ_SUCCESS) {
		return ERROR;
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
	return ERROR;		
}

void
Prober::incrementProbeInfo() {
	// store the currently probed stat
	last_mod_time = cur_probed_mod_time;
	last_size = cur_probed_size;

	last_seq_num = cur_probed_seq_num;
	last_creation_time = cur_probed_creation_time;
}
