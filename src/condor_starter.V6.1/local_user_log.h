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


#if !defined(_CONDOR_LOCAL_USER_LOG_H)
#define _CONDOR_LOCAL_USER_LOG_H

#include "condor_daemon_core.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "write_user_log.h"

/** 
	This class is used by the starter to maintain a local user log
	file for a job that runs under its control.  Each
	JobInfoCommunicator has a pointer to one of these, and if the job
	wants it for whatever reason, a LocalUserLog object is
	instantiated, and anytime the shadow would be writing a userlog
	event for this job, we do so here, too.
*/

/*
  Since JobInfoCommunicator and LocalUserLog each have a pointer to
  each other, the include files can't both include the other without
  trouble.  So, we just declare the class here, instead of including
  job_info_communicator.h, and everything is happy.
*/ 
class JobInfoCommunicator;



class LocalUserLog : public Service {
public:

		/// Constructor
	LocalUserLog( JobInfoCommunicator* my_jic );

		/// Destructor
	~LocalUserLog();

		// // // // // // // // // // // //
		// Initialization
		// // // // // // // // // // // //

		/** Initialize ourselves with the info in the given job ad.
		    If starter_ulog=true, then we will write to the log file
		    given by ATTR_STARTER_ULOG_FILE.
		    If starter_ulog=false, then we will write to the user,
		    dagman, and global event logs as part of the local universe.
		*/
	bool initFromJobAd( ClassAd* ad, bool starter_ulog = true );

		/// Do we want to be writing a log or not?
	bool wantsLog( void ) const { return should_log; };

		// // // // // // // // // // // //
		// Writing Events
		// // // // // // // // // // // //

		/** Log an execute event for this job. */
	bool logExecute( ClassAd* ad );

		/** Log a suspend event for this job.
			@param ad ClassAd containing the info we need for the
			event (which is what the JIC would be sending to the
			controller in some way)
		*/
	bool logSuspend( ClassAd* ad );

		/** Log a continue event for this job.
		*/
	bool logContinue( ClassAd* ad );

		/** Log a checkpoint event for this job.
		*/
	bool logCheckpoint( ClassAd* ad );

		/** Log an event about a fatal starter error
		 */
	bool logStarterError( const char* err_msg, bool critical );

		/** Since whenever a job exits, we want to do the same checks
			on the exit reason to decide what kind of event (terminate
			or evict) to log, everyone can just call this method,
			which will check the exit_reason and try to log the
			appropriate event for you.
		*/
	bool logJobExit( ClassAd* ad, int exit_reason );

		/** Log a terminate event for this job.
		*/
	bool logTerminate( ClassAd* ad );

		/** Log an evict event for this job.
			@param ad ClassAd containing the info we need for the
			event (which is what the JIC would be sending to the
			controller in some way)
			@param checkpointed did this job checkpoint or not?
		*/
	bool logEvict( ClassAd* ad, bool checkpointed );
	
		/**
		 * Writes an EVICT event to the user log with the requeue flag
		 * set to true. We will stuff information about why the job
		 * exited into the event object, such as the exit signal.
		 * 
		 * @param ad - the job to update the user log for
		 * @param checkpointed - whether the job was checkpointed or not
		 **/
	bool logRequeueEvent( ClassAd* ad, bool checkpointed );

private:
		// // // // // // // // // // // //
		// Private helper methods
		// // // // // // // // // // // //

		/** Since both logTerminate() and logEvict() want to include
			rusage information, we have a shared helper function to
			pull the relevent ClassAd attributes out of the ad and to
			initialize an rusage structure.
		*/ 
	struct rusage getRusageFromAd( ClassAd* ad );

		// // // // // // // // // // // //
		// Private data members
		// // // // // // // // // // // //

		/// Pointer to the jic for this LocalUserLog object. 
	JobInfoCommunicator* jic;

		/// Have we been initialized yet?
	bool is_initialized;

		/// Should we do logging?
	bool should_log;

		/// The actual UserLog object we're using to write events. 
	WriteUserLog u_log;

};


#endif /* _CONDOR_LOCAL_USER_LOG_H */
