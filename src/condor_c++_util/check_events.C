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

CheckEvents::CheckEvents(int allowEvents) :
		jobHash(JOB_HASH_SIZE, ReadMultipleUserLogs::hashFuncJobID,
		rejectDuplicateKeys)
{
	this->allowEvents = allowEvents;
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

CheckEvents::check_event_result_t
CheckEvents::CheckAnEvent(const ULogEvent *event, MyString &errorMsg)
{
	check_event_result_t	result = EVENT_OKAY;
	errorMsg = "";

		// Note: eventIsGood and notAnError are left over from when
		// this method used to return two values; I haven't propagated
		// the changes all the way down so far.  Yeah, having a
		// variable called notAnError is kind of stupid, but to
		// change it to just 'error' I'd have to make a bunch of
		// lower-level changes.  wenger, 2005-04-14.
	bool		eventIsGood = true;
	bool		notAnError = true;

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
			notAnError = false;
		}
	}

	if ( notAnError ) {
		switch ( event->eventNumber ) {
		case ULOG_SUBMIT:
			info->submitCount++;
			if ( info->submitCount != 1 ) {
				errorMsg = idStr + " submitted; submit count != 1 (" +
						MyString(info->submitCount) + ")";
				if ( !AllowAll() ) {
					notAnError = false;
				}
				eventIsGood = false;
			}
			if ( info->TotalEndCount() != 0 ) {
				errorMsg = idStr + " submitted; total end count != 0 (" +
						MyString(info->TotalEndCount()) + ")";
				if ( !AllowAll() ) {
					notAnError = false;
				}
				eventIsGood = false;
			}
			break;

		case ULOG_EXECUTE:
			if ( info->submitCount != 1 ) {
				errorMsg = idStr + " executing; submit count != 1 (" +
						MyString(info->submitCount) + ")";
				if ( (!AllowGarbage() || info->submitCount > 1) &&
							!AllowAll() ) {
					notAnError = false;
				}
				eventIsGood = false;
			}
			if ( info->TotalEndCount() != 0 ) {
				errorMsg = idStr + " executing; total end count != 0 (" +
						MyString(info->TotalEndCount()) + ")";
				if ( !AllowExtraRuns() ) {
					notAnError = false;
				}
				eventIsGood = false;
			}
			break;

		case ULOG_EXECUTABLE_ERROR:
				// Note: when we get an executable error, we seem to
				// also always get an abort.
			info->errorCount++;
				// We could probably do some extra checking here, as with
				// an execute event, but I'm not going to worry about it
				// for now.  wenger 2005-04-08.
			break;

		case ULOG_JOB_ABORTED:
			info->abortCount++;
			notAnError = CheckJobEnd(idStr, info, errorMsg, eventIsGood);
			break;

		case ULOG_JOB_TERMINATED:
			info->termCount++;
			notAnError = CheckJobEnd(idStr, info, errorMsg, eventIsGood);
			break;

		case ULOG_POST_SCRIPT_TERMINATED:
			info->postScriptCount++;
			notAnError = CheckJobEnd(idStr, info, errorMsg, eventIsGood);
			break;

		default:
			break;
		}
	}

	if ( !notAnError ) {
		result = EVENT_ERROR;
	} else if ( !eventIsGood ) {
		result = EVENT_BAD_EVENT;
	}

	return result;
}

//-----------------------------------------------------------------------------
bool
CheckEvents::CheckJobEnd(const MyString &idStr, const JobInfo *info,
		MyString &errorMsg, bool &eventIsGood)
{
	bool		result = true;

		// Allow for the case where we ran a post script after all submit
		// attempts fail.
	if ( info->submitCount == 0 && info->termCount == 0 &&
			info->postScriptCount == 1 ) {
		return result;
	}

	if ( info->submitCount != 1 ) {
		errorMsg = idStr + " ended; submit count != 1 (" +
				MyString(info->submitCount) + ")";
		if ( (!AllowGarbage() || info->submitCount > 1) && !AllowAll() ) {
			result = false;
		}
		eventIsGood = false;
	}

	if ( info->TotalEndCount() != 1 ) {
		errorMsg = idStr + " ended; total end "
				"count != 1 (" + MyString(info->TotalEndCount()) + ")";
		if ( AllowExtraAborts() &&
				(info->abortCount == 1) && (info->termCount == 1) ) {
			// Okay.
		} else if ( AllowExtraRuns() ) {
			// Okay.
		} else if ( AllowGarbage() && info->TotalEndCount() == 0 ) {
			// Okay.
		} else {
			result = false;
		}
		eventIsGood = false;
	}

	return result;
}

//-----------------------------------------------------------------------------

CheckEvents::check_event_result_t
CheckEvents::CheckAllJobs(MyString &errorMsg)
{
	check_event_result_t	result = EVENT_OKAY;
	errorMsg = "";

	bool		eventIsGood = true;
	bool		notAnError = true;

	const int	MAX_MSG_LEN = 1024;
	bool		msgFull = false; // message length has hit max

	ReadMultipleUserLogs::JobID	id;
	JobInfo *info = NULL;
	jobHash.startIterations();
	while ( jobHash.iterate(id, info) != 0 ) {

			// Put a limit on the maximum message length so we don't
			// have a chance of ending up with a ridiculously large
			// MyString...
		if ( !msgFull && (errorMsg.Length() > MAX_MSG_LEN) ) {
			errorMsg += " ...";
			msgFull = true;
		}

		char		idBuf[128];
		snprintf(idBuf, sizeof(idBuf), "%d.%d.%d", id.cluster, id.proc,
				id.subproc);
		MyString	idStr("JOB ERROR: job ");
		idStr += idBuf;
		MyString	tmpMsg;
		if ( !CheckJobEnd(idStr, info, tmpMsg, eventIsGood) ) {
			notAnError = false;
		}
		if ( !msgFull ) errorMsg += tmpMsg;
	}

	if ( !notAnError ) {
		result = EVENT_ERROR;
	} else if ( !eventIsGood ) {
		result = EVENT_BAD_EVENT;
	}

	return result;
}
