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


#ifndef _CHECK_EVENTS_H_
#define _CHECK_EVENTS_H_

#include "read_multiple_logs.h"

/** This class checks for "incorrect" events (like the two terminates for
    a single submit that caused Gnats 294).
*/

class CheckEvents {

  public:

		// This controls which "bad" events are not considered (fatal?)
		// errors.
		// ALLOW_TERM_ABORT tolerates the common Condor bug of sometimes
		// getting both a terminated and an aborted event for an aborted
		// job.
		// ALLOW_RUN_AFTER_TERM tolerates the "double-run" bug
		// (see Gnats PRs 256 and 294).
		// ALLOW_GARBAGE tolerates getting "garbage" events mixed
		// into the log (e.g., a terminated event without a corresponding
		// submit).
		// ALLOW_EXEC_BEFORE_SUBMIT tolerates getting an execute
		// event before the submit event for that job.
		// ALLOW_DOUBLE_TERMINATE tlerates getting two terminated events
		// for the same job (but does not tolerate getting an execute
		// after a terminated event).
		// WARNING: do not change existing values for event_check_allow_t,
		// since users may specify them in config parameters!!
	typedef enum {
			// All bad events are errors.
		ALLOW_NONE					= 0,

			// Almost no events are errors (the only thing considered
			// an error is execution after a terminal event).
		ALLOW_ALMOST_ALL			= 1 << 0,

			// Spurious aborted event after a terminated event is not
			// an error.
		ALLOW_TERM_ABORT			= 1 << 1,

			// "Extra" re-run of job after a terminated event is written
			// is not an error.
		ALLOW_RUN_AFTER_TERM		= 1 << 2,

			// "Garbage" (ophan) events are not errors.
		ALLOW_GARBAGE				= 1 << 3,

			// Execute before submit.  (As of 2006-07-17, this also allows
			// terminated before submit, but that's much more rare.)
		ALLOW_EXEC_BEFORE_SUBMIT	= 1 << 4,

			// Double terminated event for a single run, sometimes seen
			// for Globus jobs.
		ALLOW_DOUBLE_TERMINATE		= 1 << 5,

			// Duplicated events in general (Todd says user log semantics
			// are "write at least once", so we should be allowing
			// duplicates).
		ALLOW_DUPLICATE_EVENTS		= 1 << 6
	} check_event_allow_t;

		// This is what the checks return.
	typedef enum {
		EVENT_OKAY			= 1000,	// Everything is okay.
		EVENT_BAD_EVENT		= 1001,	// A bad event that we tolerate (don't
									// process this event at higher level).
		EVENT_ERROR			= 1002,	// A bad event we don't tolerate (error).
		EVENT_WARNING		= 1003	// Something is wrong, but the event
									// should still be processed.
	} check_event_result_t;

	/** Constructor.
	    @param What "bad" events to allow.
	*/
	CheckEvents(int allowEvents = ALLOW_NONE);

	~CheckEvents();

	/** Set the event checking level.
	    @param What "bad" events to allow.
	*/
	void SetAllowEvents(int newAllowEvents) { allowEvents = newAllowEvents; }

	/** Check an event to see if it's consistent with previous events.
		@param The event to check.
		@param A MyString to hold an error message (only relevant if
				the result value is false and/or eventIsGood is false).
		@return check_event_result_t, see above.
	*/
	check_event_result_t CheckAnEvent(const ULogEvent *event,
			MyString &errorMsg);
	check_event_result_t CheckAnEvent(const ULogEvent *event,
			std::string &errorMsg);

	/** Check all jobs when we think they're done.  Makes sure we have
		exactly one submit event and one termanated/aborted/executable
		error event for each job.
		@param A MyString to hold an error message (only relevant if
				the result value is false) (note: the error message length
				is limited, so if there are many errors they may not all
				show up).
		@return check_event_result_t, see above.
	*/
	check_event_result_t CheckAllJobs(MyString &errorMsg);
	check_event_result_t CheckAllJobs(std::string &errorMsg);

	/** Convert a check_event_result_t to the corresponding string.
		@param The result.
		@return The corresponding string.
	*/
	static const char * ResultToString(check_event_result_t resultIn);

  private:
	class JobInfo
	{
	  public:
		int		submitCount;
		int		errorCount;
		int		abortCount;
		int		termCount;
		int		postScriptCount;

		JobInfo() {
			submitCount = errorCount = abortCount = termCount =
					postScriptCount = 0;
		}

		int TotalEndCount() const { return abortCount + termCount; }
	};

	/** Check the status of a job after a submit event.
		@param A string containing this job's ID, etc. (to be used in
				any error message).
		@param The info about events we've seen for this job.
		@param A MyString to hold an error message (only relevant if
				the result value is false and/or eventIsGood is false).
		@param A reference to the result value (will be set only if
				not okay).
	*/
	void CheckJobSubmit(const MyString &idStr, const JobInfo *info,
			MyString &errorMsg, check_event_result_t &result);

	/** Check the status of a job after an execute event.
		@param A string containing this job's ID, etc. (to be used in
				any error message).
		@param The info about events we've seen for this job.
		@param A MyString to hold an error message (only relevant if
				the result value is false and/or eventIsGood is false).
		@param A reference to the result value (will be set only if
				not okay).
	*/
	void CheckJobExecute(const MyString &idStr, const JobInfo *info,
			MyString &errorMsg, check_event_result_t &result);

	/** Check the status after an event that indicates the end of a job
		(ULOG_EXECUTABLE_ERROR, ULOG_JOB_ABORTED, or ULOG_JOB_TERMINATED).
		@param A string containing this job's ID, etc. (to be used in
				any error message).
		@param The info about events we've seen for this job.
		@param A MyString to hold an error message (only relevant if
				the result value is false and/or eventIsGood is false).
		@param A reference to the result value (will be set only if
				not okay).
	*/
	void CheckJobEnd(const MyString &idStr, const JobInfo *info,
			MyString &errorMsg, check_event_result_t &result);

	/** Check the status after a ULOG_POST_SCRIPT_TERMINATED event.
		@param A string containing this job's ID, etc. (to be used in
				any error message).
		@param The job's Condor ID.
		@param The info about events we've seen for this job.
		@param A MyString to hold an error message (only relevant if
				the result value is false and/or eventIsGood is false).
		@param A reference to the result value (will be set only if
				not okay).
	*/
	void CheckPostTerm(const MyString &idStr,
			const CondorID &id, const JobInfo *info,
			MyString &errorMsg, check_event_result_t &result);

	/** Check the status of all jobs at the end of a run.
		@param A string containing this job's ID, etc. (to be used in
				any error message).
		@param The job's Condor ID.
		@param The info about events we've seen for this job.
		@param A MyString to hold an error message (only relevant if
				the result value is false and/or eventIsGood is false).
		@param A reference to the result value (will be set only if
				not okay).
	*/
	void CheckJobFinal(const MyString &idStr,
			const CondorID &id, const JobInfo *info,
			MyString &errorMsg, check_event_result_t &result);

		// Map Condor ID to a JobInfo object.  This hash table will have
		// one entry for each Condor job we process.
	HashTable<CondorID, JobInfo *>	jobHash;

	inline bool		AllowAlmostAll() const { return allowEvents & ALLOW_ALMOST_ALL; }

	inline bool		AllowExtraAborts() const { return
			(allowEvents & ALLOW_ALMOST_ALL) ||
			(allowEvents & ALLOW_TERM_ABORT); }

	inline bool		AllowExtraRuns() const { return
			allowEvents & ALLOW_RUN_AFTER_TERM; }

	inline bool		AllowGarbage() const { return (allowEvents & ALLOW_ALMOST_ALL) ||
			(allowEvents & ALLOW_GARBAGE); }

	inline bool		AllowExecSubmit() const { return
			(allowEvents & ALLOW_ALMOST_ALL) ||
			(allowEvents & ALLOW_EXEC_BEFORE_SUBMIT); }

	inline bool		AllowDoubleTerm() const { return
			(allowEvents & ALLOW_ALMOST_ALL) ||
			(allowEvents & ALLOW_DOUBLE_TERMINATE); }

	inline bool		AllowDuplicates() const { return
			(allowEvents & ALLOW_ALMOST_ALL) ||
			allowEvents & ALLOW_DUPLICATE_EVENTS; }

		// Bit-mapped flag for what "bad" events to allow.
	int		allowEvents;

		// Special Condor ID for POST script run after submit failures.
	CondorID	noSubmitId;

	// For instantiation in programs that use this class.
#define CHECK_EVENTS_HASH_INSTANCE template class \
		HashTable<CondorID, CheckEvents::JobInfo *>;

};

#endif // CHECK_EVENTS_H
