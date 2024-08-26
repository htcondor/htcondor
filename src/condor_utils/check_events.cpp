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


#include "condor_common.h"
#include "check_events.h"
#include "read_multiple_logs.h"
#include "stl_string_utils.h"

//-----------------------------------------------------------------------------

CheckEvents::CheckEvents(int allowEventsSetting) :
		noSubmitId(-1, 0, 0)
{
	allowEvents = allowEventsSetting;
}

//-----------------------------------------------------------------------------

CheckEvents::~CheckEvents()
{
}

//-----------------------------------------------------------------------------

CheckEvents::check_event_result_t
CheckEvents::CheckAnEvent(const ULogEvent *event, std::string &errorMsg)
{
	check_event_result_t	result = EVENT_OKAY;
	errorMsg = "";

	CondorID	id(event->cluster, event->proc, event->subproc);

	std::string idStr("BAD EVENT: job ");
	formatstr_cat(idStr, "(%d.%d.%d)", id._cluster, id._proc, id._subproc);

	// If the map already has an entry for id, then it will return an
	// iterator to the existing entry.
	auto [it, success] = jobHash.insert({id, JobInfo()});
	JobInfo& info = it->second;

	if ( result != EVENT_ERROR ) {
		switch ( event->eventNumber ) {
		case ULOG_SUBMIT:
			info.submitCount++;
			CheckJobSubmit(idStr, info, errorMsg, result);
			break;

		case ULOG_EXECUTE:
			CheckJobExecute(idStr, info, errorMsg, result);
			break;

		case ULOG_EXECUTABLE_ERROR:
				// Note: when we get an executable error, we seem to
				// also always get an abort.
			info.errorCount++;
				// We could probably do some extra checking here, as with
				// an execute event, but I'm not going to worry about it
				// for now.  wenger 2005-04-08.
			break;

		case ULOG_JOB_ABORTED:
			info.abortCount++;
			CheckJobEnd(idStr, info, errorMsg, result);
			break;

		case ULOG_JOB_TERMINATED:
			info.termCount++;
			CheckJobEnd(idStr, info, errorMsg, result);
			break;

		case ULOG_POST_SCRIPT_TERMINATED:
			info.postScriptCount++;
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
CheckEvents::CheckJobSubmit(const std::string &idStr, const JobInfo &info,
		std::string &errorMsg, check_event_result_t &result)
{
	if ( info.submitCount != 1 ) {
		formatstr( errorMsg, "%s submitted, submit count != 1 (%d)",
		           idStr.c_str(), info.submitCount );
		result = AllowDuplicates() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}

	if ( info.TotalEndCount() != 0 ) {
		formatstr( errorMsg, "%s submitted, total end count != 0 (%d)",
		           idStr.c_str(), info.TotalEndCount() );
		result = AllowExecSubmit() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}
}

//-----------------------------------------------------------------------------
void
CheckEvents::CheckJobExecute(const std::string &idStr, const JobInfo &info,
		std::string &errorMsg, check_event_result_t &result)
{
	if ( info.submitCount < 1 ) {
		formatstr( errorMsg, "%s executing, submit count < 1 (%d)",
		           idStr.c_str(), info.submitCount );
		result = (AllowExecSubmit() || AllowGarbage()) ?
				EVENT_WARNING : EVENT_ERROR;
	}

	if ( info.TotalEndCount() != 0 ) {
		formatstr( errorMsg, "%s executing, total end count != 0 (%d)",
		           idStr.c_str(), info.TotalEndCount() );
		result = AllowExtraRuns() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}
}

//-----------------------------------------------------------------------------
void
CheckEvents::CheckJobEnd(const std::string &idStr, const JobInfo &info,
		std::string &errorMsg, check_event_result_t &result)
{
	if ( info.submitCount < 1 ) {
		formatstr( errorMsg, "%s ended, submit count < 1 (%d)",
		           idStr.c_str(), info.submitCount );
		result = (AllowExecSubmit() ||
				(AllowGarbage() && (info.submitCount <= 1))) ?
				EVENT_WARNING : EVENT_ERROR;
	}

	if ( info.TotalEndCount() != 1 ) {
		formatstr( errorMsg, "%s ended, total end count != 1 (%d)",
		           idStr.c_str(), info.TotalEndCount() );
		if ( AllowExtraAborts() &&
				(info.abortCount == 1) && (info.termCount == 1) ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowDoubleTerm() && (info.termCount == 2) ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowExtraRuns() ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowDuplicates() ) {
				// Note: should we check here that we don't have some
				// weird combo that *should* be illegal?
			result = EVENT_BAD_EVENT;
		} else {
			result = EVENT_ERROR;
		}
	}

	if ( info.postScriptCount != 0 ) {
		formatstr( errorMsg, "%s ended, post script count != 0 (%d)",
		           idStr.c_str(), info.postScriptCount );
		result = AllowDuplicates() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}
}

//-----------------------------------------------------------------------------
void
CheckEvents::CheckPostTerm(const std::string &idStr,
		const CondorID &id, const JobInfo &info,
		std::string &errorMsg, check_event_result_t &result)
{
		// Allow for the case where we ran a post script after all submit
		// attempts fail.
	if ( noSubmitId == id && info.submitCount == 0 && info.termCount == 0 &&
			info.postScriptCount >= 1 ) {
		return;
	}

	if ( info.submitCount < 1 ) {
		formatstr( errorMsg, "%s post script ended, submit count < 1 (%d)",
		           idStr.c_str(), info.submitCount );
		result = (AllowDuplicates() ||
				(AllowGarbage() && (info.submitCount <= 1))) ?
				EVENT_BAD_EVENT : EVENT_ERROR;
	}

	if ( info.TotalEndCount() < 1 ) {
		formatstr( errorMsg, "%s post script ended, total end count < 1 (%d)",
				   idStr.c_str(), info.TotalEndCount() );
		result = AllowAlmostAll() ? EVENT_BAD_EVENT : EVENT_ERROR;
	}

	if ( info.postScriptCount > 1 ) {
		formatstr( errorMsg, "%s post script ended, post script count > 1 (%d)",
		           idStr.c_str(), info.postScriptCount );
		result = (AllowDuplicates() || AllowGarbage()) ?
				EVENT_BAD_EVENT : EVENT_ERROR;
	}
}

//-----------------------------------------------------------------------------
void
CheckEvents::CheckJobFinal(const std::string &idStr,
		const CondorID &id, const JobInfo &info,
		std::string &errorMsg, check_event_result_t &result)
{
		// Allow for the case where we ran a post script after all submit
		// attempts fail.
	if ( noSubmitId == id && info.submitCount == 0 && info.termCount == 0 &&
			info.postScriptCount >= 1 ) {
		return;
	}

	if ( id._subproc != 0 ) {
			// This is a parallel universe node.
		return;
	}

	if ( info.submitCount != 1 ) {
		formatstr( errorMsg, "%s ended, submit count != 1 (%d)",
		           idStr.c_str(), info.submitCount );
		result = (AllowAlmostAll() ||
				(AllowGarbage() && (info.submitCount <= 1))) ?
				EVENT_BAD_EVENT : EVENT_ERROR;
	}

	if ( info.TotalEndCount() != 1 ) {
		formatstr( errorMsg, "%s ended, total end count != 1 (%d)",
		           idStr.c_str(), info.TotalEndCount() );
		if ( AllowExtraAborts() &&
				(info.abortCount == 1) && (info.termCount == 1) ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowDoubleTerm() && (info.termCount == 2) ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowExtraRuns() ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowGarbage() && info.TotalEndCount() == 0 ) {
			result = EVENT_BAD_EVENT;
		} else if ( AllowDuplicates() ) {
				// Note: should we check here that we don't have some
				// weird combo that *should* be illegal?
			result = EVENT_BAD_EVENT;
		} else {
			result = EVENT_ERROR;
		}
	}

	if ( info.postScriptCount > 1 ) {
		formatstr( errorMsg, "%s ended, post script count > 1 (%d)",
		           idStr.c_str(), info.postScriptCount );
		result = (AllowDuplicates() || AllowGarbage()) ?
				EVENT_BAD_EVENT : EVENT_ERROR;
	}
}

//-----------------------------------------------------------------------------

CheckEvents::check_event_result_t
CheckEvents::CheckAllJobs(std::string &errorMsg)
{
	check_event_result_t	result = EVENT_OKAY;
	errorMsg = "";

	const int	MAX_MSG_LEN = 1024;
	bool		msgFull = false; // message length has hit max

	for (auto& [id, info] : jobHash) {

			// Put a limit on the maximum message length so we don't
			// have a chance of ending up with a ridiculously large
			// string...
		if ( !msgFull && (errorMsg.length() > MAX_MSG_LEN) ) {
			errorMsg += " ...";
			msgFull = true;
		}

		std::string idStr("BAD EVENT: job ");
		formatstr_cat(idStr, "(%d.%d.%d)", id._cluster, id._proc, id._subproc);

		std::string tmpMsg;
		CheckJobFinal(idStr, id, info, tmpMsg, result);
		if ( tmpMsg != "" && !msgFull ) {
			if ( errorMsg != "" ) errorMsg += "; ";
			errorMsg += tmpMsg;
		}
	}

	return result;
}

//-----------------------------------------------------------------------------

const char *
CheckEvents::ResultToString(check_event_result_t resultIn)
{
	const char *	resultStr = NULL;

	switch ( resultIn ) {
	case EVENT_OKAY:
		resultStr = "EVENT_OKAY";
		break;

	case EVENT_BAD_EVENT:
		resultStr = "EVENT_BAD_EVENT";
		break;

	case EVENT_ERROR:
		resultStr = "EVENT_ERROR";
		break;

	default:
		resultStr = "Bad result value!!!!";
		break;
	}

	return resultStr;
}
