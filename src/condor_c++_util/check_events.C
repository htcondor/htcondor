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
#include "check_events.h"
#include "read_multiple_logs.h"

#define JOB_HASH_SIZE 101 // prime

//-----------------------------------------------------------------------------

CheckEvents::CheckEvents() :
		jobHash(JOB_HASH_SIZE, ReadMultipleUserLogs::hashFuncJobID,
		rejectDuplicateKeys)
{
}

//-----------------------------------------------------------------------------

CheckEvents::~CheckEvents()
{
	JobInfo *info = NULL;
	jobHash.startIterations();
	while ( jobHash.iterate(info) != 0 ) {
		delete info;
	}
	jobHash.clear();
}

//-----------------------------------------------------------------------------

bool
CheckEvents::CheckAnEvent(const ULogEvent *event, MyString &errorMsg)
{
	bool		result = true;
	errorMsg = "";

	ReadMultipleUserLogs::JobID	id;
	id.cluster = event->cluster;
	id.proc = event->proc;
	id.subproc = event->subproc;

	char		idBuf[128];
	snprintf(idBuf, sizeof(idBuf), "%d.%d.%d", id.cluster, id.proc, id.subproc);
	MyString	idStr("EVENT ERROR: job ");
	idStr += idBuf;

	JobInfo *info = NULL;
	if ( jobHash.lookup(id, info) == 0 ) {
			// We already have an entry for this ID.
	} else {
			// Insert an entry for this ID.
		info = new JobInfo();
		if ( jobHash.insert(id, info) != 0 ) {
			errorMsg = "EVENT ERROR: hash table insert error";
			result = false;
		}
	}

	if ( result ) {
		switch ( event->eventNumber ) {
		case ULOG_SUBMIT:
			info->submitCount++;
			if ( info->submitCount != 1 ) {
				errorMsg = idStr + " submitted; submit count != 1 (" +
						MyString(info->submitCount) + ")";
				result = false;
			}
			if ( info->termAbortCount != 0 ) {
				errorMsg = idStr + " submitted; abort/terminate count != 0 (" +
						MyString(info->termAbortCount) + ")";
				result = false;
			}
			break;

		case ULOG_EXECUTE:
			if ( info->submitCount != 1 ) {
				errorMsg = idStr + " executing; submit count != 1 (" +
						MyString(info->submitCount) + ")";
				result = false;
			}
			if ( info->termAbortCount != 0 ) {
				errorMsg = idStr + " executing; abort/terminate count != 0 (" +
						MyString(info->termAbortCount) + ")";
				result = false;
			}
			break;

		case ULOG_EXECUTABLE_ERROR:
		case ULOG_JOB_TERMINATED:
		case ULOG_JOB_ABORTED:
			info->termAbortCount++;
			if ( info->submitCount != 1 ) {
				errorMsg = idStr + " aborted or terminated; submit count != 1 (" +
						MyString(info->submitCount) + ")";
				result = false;
			}
			if ( info->termAbortCount != 1 ) {
				errorMsg = idStr + " aborted or terminated; abort/terminate count != 1 (" +
						MyString(info->termAbortCount) + ")";
				result = false;
			}
			break;

		default:
			break;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------

bool
CheckEvents::CheckAllJobs(MyString &errorMsg)
{
	bool		result = true;
	errorMsg = "";
	const int	MAX_MSG_LEN = 1024;

	ReadMultipleUserLogs::JobID	id;
	JobInfo *info = NULL;
	jobHash.startIterations();
	while ( jobHash.iterate(id, info) != 0 ) {

			// Put a limit on the maximum message length so we don't have a chance
			// of ending up with a ridiculously large MyString...
		if ( errorMsg.Length() > MAX_MSG_LEN ) {
			errorMsg += " ...";
			break;
		}

		if ( info->submitCount != 1 ) {
			char		idBuf[128];
			snprintf(idBuf, sizeof(idBuf), "%d.%d.%d", id.cluster, id.proc, id.subproc);
			errorMsg += MyString("JOB ERROR: job ") + idBuf + " has submit count != 1 (" +
					MyString(info->submitCount) + "); ";
			result = false;
		}
		if ( info->termAbortCount != 1 ) {
			char		idBuf[128];
			snprintf(idBuf, sizeof(idBuf), "%d.%d.%d", id.cluster, id.proc, id.subproc);
			errorMsg += MyString("JOB ERROR: job ") + idBuf +
					" has terminate/abort count != 1 (" +
					MyString(info->termAbortCount) + "); ";
			result = false;
		}
	}

	return result;
}
