/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
			rejectDuplicateKeys),
		noSubmitId(-1, 0, 0)
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

	CondorID	id(event->cluster, event->proc, event->subproc);

	MyString	idStr("BAD EVENT: job ");
	idStr.sprintf_cat("(%d.%d.%d)", id._cluster, id._proc, id._subproc);

	JobInfo *info = NULL;
	if ( jobHash.lookup(id, info) == 0 ) {
			// We already have an entry for this ID.
	} else {
			// Insert an entry for this ID.
		info = new JobInfo();
		if ( jobHash.insert(id, info) != 0 ) {
			errorMsg = "EVENT ERROR: hash table insert error";
			result = EVENT_ERROR;
		}
	}

	if ( result != EVENT_ERROR ) {
		switch ( event->eventNumber ) {
		case ULOG_SUBMIT:
			info->submitCount++;
			CheckJobSubmit(idStr, info, errorMsg, result);
			break;

		case ULOG_EXECUTE:
			CheckJobExecute(idStr, info, errorMsg, result);
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
			CheckJobEnd(idStr, info, errorMsg, result);
			break;

		case ULOG_JOB_TERMINATED:
			info->termCount++;
			CheckJobEnd(idStr, info, errorMsg, result);
			break;

		case ULOG_POST_SCRIPT_TERMINATED:
			info->postScriptCount++;
			CheckPostTerm(idStr, id, info, errorMsg, result);
			break;

		default:
			break;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------
void
CheckEvents::CheckJobSubmit(const MyString &idStr, const JobInfo *info,
		MyString &errorMsg, check_event_result_t &result)
{
	if ( info->submitCount != 1 ) {
		errorMsg = idStr + " submitted, submit count != 1 (" +
				MyString(info->submitCount) + ")";
		result = AllowAll() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}

	if ( info->TotalEndCount() != 0 ) {
		errorMsg = idStr + " submitted, total end count != 0 (" +
				MyString(info->TotalEndCount()) + ")";
		result = AllowAll() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}
}

//-----------------------------------------------------------------------------
void
CheckEvents::CheckJobExecute(const MyString &idStr, const JobInfo *info,
		MyString &errorMsg, check_event_result_t &result)
{
	if ( info->submitCount != 1 ) {
		errorMsg = idStr + " executing, submit count != 1 (" +
				MyString(info->submitCount) + ")";
		result = ((AllowExecSubmit() && (info->submitCount == 0)) ||
				(AllowGarbage() && (info->submitCount <= 1))) ?
				EVENT_BAD_EVENT : EVENT_ERROR;
	}

	if ( info->TotalEndCount() != 0 ) {
		errorMsg = idStr + " executing, total end count != 0 (" +
				MyString(info->TotalEndCount()) + ")";
		result = AllowExtraRuns() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}
}

//-----------------------------------------------------------------------------
void
CheckEvents::CheckJobEnd(const MyString &idStr, const JobInfo *info,
		MyString &errorMsg, check_event_result_t &result)
{
	if ( info->submitCount != 1 ) {
		errorMsg = idStr + " ended, submit count != 1 (" +
				MyString(info->submitCount) + ")";
		result = (AllowAll() || (AllowGarbage() && (info->submitCount <= 1))) ?
				EVENT_BAD_EVENT : EVENT_ERROR;
	}

	if ( info->TotalEndCount() != 1 ) {
		errorMsg = idStr + " ended, total end "
				"count != 1 (" + MyString(info->TotalEndCount()) + ")";
		if ( AllowExtraAborts() &&
				(info->abortCount == 1) && (info->termCount == 1) ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowDoubleTerm() && (info->termCount == 2) ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowExtraRuns() ) {
			result = EVENT_BAD_EVENT;
		} else {
			result = EVENT_ERROR;
		}
	}

	if ( info->postScriptCount != 0 ) {
		errorMsg = idStr + " ended, post script "
				"count != 0 (" + MyString(info->postScriptCount) + ")";
		result = AllowAll() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}
}

//-----------------------------------------------------------------------------
void
CheckEvents::CheckPostTerm(const MyString &idStr,
		const CondorID &id, const JobInfo *info,
		MyString &errorMsg, check_event_result_t &result)
{
		// Allow for the case where we ran a post script after all submit
		// attempts fail.
	if ( noSubmitId == id && info->submitCount == 0 && info->termCount == 0 &&
			info->postScriptCount >= 1 ) {
		return;
	}

	if ( info->submitCount != 1 ) {
		errorMsg = idStr + " post script ended, submit count != 1 (" +
				MyString(info->submitCount) + ")";
		result = (AllowAll() || (AllowGarbage() && (info->submitCount <= 1))) ?
				EVENT_BAD_EVENT : EVENT_ERROR;
	}

	if ( info->TotalEndCount() < 1 ) {
		errorMsg = idStr + " post script ended, total end "
				"count < 1 (" + MyString(info->TotalEndCount()) + ")";
		result = AllowAll() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}

	if ( info->postScriptCount > 1 ) {
		errorMsg = idStr + " post script ended, post script "
				"count > 1 (" + MyString(info->postScriptCount) + ")";
		result = AllowGarbage() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}
}

//-----------------------------------------------------------------------------
void
CheckEvents::CheckJobFinal(const MyString &idStr,
		const CondorID &id, const JobInfo *info,
		MyString &errorMsg, check_event_result_t &result)
{
		// Allow for the case where we ran a post script after all submit
		// attempts fail.
	if ( noSubmitId == id && info->submitCount == 0 && info->termCount == 0 &&
			info->postScriptCount >= 1 ) {
		return;
	}

	if ( info->submitCount != 1 ) {
		errorMsg = idStr + " ended, submit count != 1 (" +
				MyString(info->submitCount) + ")";
		result = (AllowAll() || (AllowGarbage() && (info->submitCount <= 1))) ?
				EVENT_BAD_EVENT : EVENT_ERROR;
	}

	if ( info->TotalEndCount() != 1 ) {
		errorMsg = idStr + " ended, total end "
				"count != 1 (" + MyString(info->TotalEndCount()) + ")";
		if ( AllowExtraAborts() &&
				(info->abortCount == 1) && (info->termCount == 1) ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowDoubleTerm() && (info->termCount == 2) ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowExtraRuns() ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowGarbage() && info->TotalEndCount() == 0 ) {
			result = EVENT_BAD_EVENT;
		} else {
			result = EVENT_ERROR;
		}
	}

	if ( info->postScriptCount > 1 ) {
		errorMsg = idStr + " ended, post script "
				"count > 1 (" + MyString(info->postScriptCount) + ")";
		result = AllowGarbage() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}
}

//-----------------------------------------------------------------------------

CheckEvents::check_event_result_t
CheckEvents::CheckAllJobs(MyString &errorMsg)
{
	check_event_result_t	result = EVENT_OKAY;
	errorMsg = "";

	const int	MAX_MSG_LEN = 1024;
	bool		msgFull = false; // message length has hit max

	CondorID	id;
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

		MyString	idStr("BAD EVENT: job ");
		idStr.sprintf_cat("(%d.%d.%d)", id._cluster, id._proc, id._subproc);

		MyString	tmpMsg;
		CheckJobFinal(idStr, id, info, tmpMsg, result);
		if ( tmpMsg != "" && !msgFull ) {
			if ( errorMsg != "" ) errorMsg += "; ";
			errorMsg += tmpMsg;
		}
	}

	return result;
}
