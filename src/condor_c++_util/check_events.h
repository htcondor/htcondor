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
	/** Constructor.
	    @param Whether to allow "extra" abort events.
		@param Whether to allow "extra" runs (Condor errors).
		@param Whether to allow "garbage" events (submit with no
		       corresponding terminate, vice-versa, etc.).
	*/
	CheckEvents(bool allowExtraAborts = false, bool allowExtraRuns = false,
			bool allowGarbage = false);

	~CheckEvents();

	/** Check an event to see if it's consistent with previous events.
		Note that if we have set things up to tolerate certain "bad"
		events, the return value can be true, but eventIsGood is false.
		@param The event to check.
		@param A MyString to hold an error message (only relevant if
				the result value is false and/or eventIsGood is false).
		@param Whether the event is "good" (set by this method).
		@return true on success, false on failure ("incorrect" event).
	*/
	bool CheckAnEvent(const ULogEvent *event, MyString &errorMsg,
			bool &eventIsGood);

	/** Check all jobs when we think they're done.  Makes sure we have
		exactly one submit event and one termanated/aborted/executable
		error event for each job.
		@param A MyString to hold an error message (only relevant if
				the result value is false) (note: the error message length
				is limited, so if there are many errors they may not all
				show up).
		@return true on success, false on failure.
	*/
	bool CheckAllJobs(MyString &errorMsg);

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

	/** Check the status after an event that indicates the end of a job
		(ULOG_EXECUTABLE_ERROR, ULOG_JOB_ABORTED, or ULOG_JOB_TERMINATED).
		@param A string containing this job's ID, etc. (to be used in
				any error message).
		@param The info about events we've seen for this job.
		@param A MyString to hold an error message (only relevant if
				the result value is false and/or eventIsGood is false).
		@param Whether the event is "good" (set by this method)
		@return true on success, false on failure
	*/
	bool CheckJobEnd(const MyString &idStr, const JobInfo *info,
			MyString &errorMsg, bool &eventIsGood);

	HashTable<ReadMultipleUserLogs::JobID, JobInfo *>	jobHash;

		// Allow an abort event after a normal termination.
	bool		allowExtraAborts;

		// Allow more than one run for a single submit.
	bool		allowExtraRuns;

		// Allow "garbage" events (e.g., a submit with no corresponding
		// terminated, or vice-versa).
	bool		allowGarbage;

	// For instantiation in programs that use this class.
#define CHECK_EVENTS_HASH_INSTANCE template class \
		HashTable<ReadMultipleUserLogs::JobID, CheckEvents::JobInfo *>;

};

#endif // CHECK_EVENTS_H
