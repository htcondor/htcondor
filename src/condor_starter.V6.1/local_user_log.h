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

#if !defined(_CONDOR_LOCAL_USER_LOG_H)
#define _CONDOR_LOCAL_USER_LOG_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_classad.h"
#include "user_log.c++.h"

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

		/** Initialize ourselves with all the info we need already
			proccessed and handed to use directly
			@param filename Full path to userlog to write
		*/
	bool init( const char* filename, bool is_xml, 
			   int cluster, int proc, int subproc );

		/** Initialize ourselves with the info in the given job ad.
		*/
	bool initFromJobAd( ClassAd* ad );

		/// Initialize ourselves such that we won't write a user log 
	bool initNoLogging( void );

		/// Do we want to be writing a log or not?
	bool wantsLog( void ) { return should_log; };

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


private:

		// // // // // // // // // // // //
		// Private helper methods
		// // // // // // // // // // // //

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
	UserLog u_log;

};


#endif /* _CONDOR_LOCAL_USER_LOG_H */
