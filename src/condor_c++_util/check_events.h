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
	CheckEvents();
	~CheckEvents();

	/** Check an event to see if it's consistent with previous events.
	    @param The event to check
	    @param A MyString to hold an error message (only relevant if
		       the result value is false)
	    @return true on success, false on failure ("incorrect" event)
	*/
	bool CheckAnEvent(const ULogEvent *event, MyString &errorMsg);

	/** Check all jobs when we think they're done.  Makes sure we have
	    exactly one submit event and one termanated/aborted/executable
		error event for each job.
	    @param A MyString to hold an error message (only relevant if
		       the result value is false) (note: the error message length
			   is limited, so if there are many errors they may not all
			   show up)
	    @return true on success, false on failure
	*/
	bool CheckAllJobs(MyString &errorMsg);

  private:
	class JobInfo
	{
	public:
		int		submitCount;
		int		termAbortCount;

		JobInfo() {
			submitCount = termAbortCount = 0;
		}
	};

	HashTable<ReadMultipleUserLogs::JobID, JobInfo *>	jobHash;

	// For instantiation in programs that use this class.
#define CHECK_EVENTS_HASH_INSTANCE template class \
		HashTable<ReadMultipleUserLogs::JobID, CheckEvents::JobInfo *>;

};

#endif // CHECK_EVENTS_H
