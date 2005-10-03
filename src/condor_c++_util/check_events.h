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

			// No events are errors.
		ALLOW_ALL					= 1 << 0,

			// Spurious aborted event after a terminated event is not
			// an error.
		ALLOW_TERM_ABORT			= 1 << 1,

			// "Extra" re-run of job after a terminated event is written
			// is not an error.
		ALLOW_RUN_AFTER_TERM		= 1 << 2,

			// "Garbage" (ophan) events are not errors.
		ALLOW_GARBAGE				= 1 << 3,

			// Execute before submit.
		ALLOW_EXEC_BEFORE_SUBMIT	= 1 << 4,

			// Double terminated event for a single run, sometimes seen
			// for Globus jobs.
		ALLOW_DOUBLE_TERMINATE		= 1 << 5
	} check_event_allow_t;

		// This is what the checks return.
	typedef enum {
		EVENT_OKAY			= 1000,	// Everything is okay.
		EVENT_BAD_EVENT		= 1001,	// A bad event that we tolerate.
		EVENT_ERROR			= 1002	// A bad event we don't tolerate (error).
	} check_event_result_t;

	/** Constructor.
	    @param What "bad" events to allow.
	*/
	CheckEvents(int allowEvents = ALLOW_NONE);

	~CheckEvents();

	/** Check an event to see if it's consistent with previous events.
		@param The event to check.
		@param A MyString to hold an error message (only relevant if
				the result value is false and/or eventIsGood is false).
		@return check_event_result_t, see above.
	*/
	check_event_result_t CheckAnEvent(const ULogEvent *event,
			MyString &errorMsg);

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

	HashTable<CondorID, JobInfo *>	jobHash;

	inline bool		AllowAll() { return allowEvents & ALLOW_ALL; }

	inline bool		AllowExtraAborts() { return (allowEvents & ALLOW_ALL) ||
			(allowEvents & ALLOW_TERM_ABORT); }

	inline bool		AllowExtraRuns() { return (allowEvents & ALLOW_ALL) ||
			(allowEvents & ALLOW_RUN_AFTER_TERM); }

	inline bool		AllowGarbage() { return (allowEvents & ALLOW_ALL) ||
			(allowEvents & ALLOW_GARBAGE); }

	inline bool		AllowExecSubmit() { return (allowEvents & ALLOW_ALL) ||
			(allowEvents & ALLOW_EXEC_BEFORE_SUBMIT); }

	inline bool		AllowDoubleTerm() { return (allowEvents & ALLOW_ALL) ||
			(allowEvents & ALLOW_DOUBLE_TERMINATE); }

		// Bit-mapped flag for what "bad" events to allow.
	int		allowEvents;

		// Special Condor ID for POST script run after submit failures.
	CondorID	noSubmitId;

	// For instantiation in programs that use this class.
#define CHECK_EVENTS_HASH_INSTANCE template class \
		HashTable<CondorID, CheckEvents::JobInfo *>;

};

#endif // CHECK_EVENTS_H
