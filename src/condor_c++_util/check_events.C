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

CheckEvents::CheckEvents(bool allowExtraAborts, bool allowExtraRuns) :
		jobHash(JOB_HASH_SIZE, ReadMultipleUserLogs::hashFuncJobID,
		rejectDuplicateKeys)
{
	this->allowExtraAborts = allowExtraAborts;
	this->allowExtraRuns = allowExtraRuns;
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
CheckEvents::CheckAnEvent(const ULogEvent *event, MyString &errorMsg,
		bool &eventIsGood)
{
	eventIsGood = true;
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
				eventIsGood = false;
			}
			if ( info->TotalEndCount() != 0 ) {
				errorMsg = idStr + " submitted; total end count != 0 (" +
						MyString(info->TotalEndCount()) + ")";
				result = false;
				eventIsGood = false;
			}
			break;

		case ULOG_EXECUTE:
			if ( info->submitCount != 1 ) {
				errorMsg = idStr + " executing; submit count != 1 (" +
						MyString(info->submitCount) + ")";
				result = false;
				eventIsGood = false;
			}
			if ( info->TotalEndCount() != 0 ) {
				errorMsg = idStr + " executing; total end count != 0 (" +
						MyString(info->TotalEndCount()) + ")";
				if ( !allowExtraRuns ) {
					result = false;
				}
				eventIsGood = false;
			}
			break;

		case ULOG_EXECUTABLE_ERROR:
			info->errorCount++;
			result = CheckJobEnd(idStr, info, errorMsg, eventIsGood);
			break;

		case ULOG_JOB_ABORTED:
			info->abortCount++;
			result = CheckJobEnd(idStr, info, errorMsg, eventIsGood);
			break;

		case ULOG_JOB_TERMINATED:
			info->termCount++;
			result = CheckJobEnd(idStr, info, errorMsg, eventIsGood);
			break;

		default:
			break;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
bool
CheckEvents::CheckJobEnd(const MyString &idStr, const JobInfo *info,
		MyString &errorMsg, bool &eventIsGood)
{
	bool		result = true;

	if ( info->submitCount != 1 ) {
		errorMsg = idStr + " ended; submit count != 1 (" +
				MyString(info->submitCount) + ")";
		result = false;
		eventIsGood = false;
	}

	if ( info->TotalEndCount() != 1 ) {
		errorMsg = idStr + " ended; total end "
				"count != 1 (" + MyString(info->TotalEndCount()) + ")";
		if ( allowExtraAborts &&
				(info->abortCount == 1) && (info->termCount == 1) ) {
			// Okay.
		} else if ( allowExtraRuns ) {
			// Okay.
		} else {
			result = false;
		}
		eventIsGood = false;
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

			// Put a limit on the maximum message length so we don't
			// have a chance of ending up with a ridiculously large
			// MyString...
		if ( errorMsg.Length() > MAX_MSG_LEN ) {
			errorMsg += " ...";
			break;
		}

		char		idBuf[128];
		snprintf(idBuf, sizeof(idBuf), "%d.%d.%d", id.cluster, id.proc,
				id.subproc);
		MyString	idStr("JOB ERROR: job ");
		idStr += idBuf;
		bool		eventIsGood; // dummy
		if ( !CheckJobEnd(idStr, info, errorMsg, eventIsGood) ) {
			result = false;
		}
	}

	return result;
}
