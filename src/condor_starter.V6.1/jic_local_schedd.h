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


#if !defined(_CONDOR_JIC_LOCAL_SCHEDD_H)
#define _CONDOR_JIC_LOCAL_SCHEDD_H

#include "jic_local_file.h"
#include <qmgr_job_updater.h>
#include "starter_user_policy.h"

/** 
	This is the child class of JICLocalFile (and therefore JICLocal
	and JobInfoCommunicator) that deals with running "local universe"
	jobs directly under a condor_schedd.  This JIC gets the job
	ClassAd info from a file (a pipe to STDIN, in fact).  Instead of
	simply reporting everything to a file, it reports info back to the
	schedd via special exit status codes.
*/

class JICLocalSchedd : public JICLocalFile {
public:

		/** Constructor 
			@param classad_filename Full path to the ClassAd, "-" if STDIN
			@param schedd_address Sinful string of the schedd's qmgmt port
			@param cluster Cluster ID number (if any)
			@param proc Proc ID number (if any)
			@param subproc Subproc ID number (if any)
		*/
	JICLocalSchedd( const char* classad_filename,
					const char* schedd_address,
					int cluster, int proc, int subproc );

		/// Destructor
	virtual ~JICLocalSchedd();

		/// Initialize ourselves
		/// This first will call JICLocal::init() then
		/// initialize the user policy object
	virtual bool init( void );
	
		/**
		 * Puts the job on hold. The Starter actually does the 
		 * dirty work, we just add the reason to the ad and email the
		 * user. We set the exit_code to JOB_SHOULD_HOLD so that
		 * schedd will put the job on hold in the queue. We do NOT need
		 * to write an EVICT log event because JICLocalSchedd::notifyJobExit()
		 * will take care of that for us.
		 * 
		 * @param reason - why the job is going on hold
		 * @return true if the jobs were told to be put on hold
		 */
	virtual bool holdJob( const char*, int hold_reason_code, int hold_reason_subcode );
	
		/**
		 * The job needs to be removed from the Starter. This
		 * is more than just calling Starter->Remove() because we need
		 * to stuff the remove reason in the job ad and update the job queue.
		 * We also set the proper exit code for the Starter. We do NOT need
		 * to write an EVICT log event because JICLocalSchedd::notifyJobExit()
		 * will take care of that for us.
		 * 
		 * @param reason - why the job is being removed
		 * @return true if the job was set to be removed
		 **/
	virtual bool removeJob( const char* );
	
		/*
		 * The job exited on its own accord and its not to be requeued.
		 * 
		 * @param reason - why the job is being terminated
		 * @return true if the job was set to be terminated
		 **/
	virtual bool terminateJob( const char* );
	
		/**
		 * The job needs to be requeued back on the schedd, so 
		 * we need to update the job ad, the queue, and write 
		 * a REQUEUE event into the user log. We have to set
		 * the proper exit code for the Starter so that the schedd
		 * knows to put the job back on hold.
		 * 
		 * @param reason - why the job is being requeued
		 * @return true if the job was set to be requeued
		 **/
	virtual bool requeueJob( const char* );

	virtual void allJobsGone( void );

		/// The starter has been asked to shutdown fast.
	virtual void gotShutdownFast( void );

		/// The starter has been asked to shutdown gracefully.
	virtual void gotShutdownGraceful( void );

		/// The starter has been asked to evict for condor_rm
	virtual void gotRemove( void );

		/// The starter has been asked to evict for condor_hold
	virtual void gotHold( void );

		/** Get the local job ad.  We need a JICLocalSchedd specific
			version of this so that after we grab the ad (using the
			JICLocal version), we need to initialize our
			QmgrJobUpdater object with a pointer to the ad.
		*/
	virtual bool getLocalJobAd( void );

		/**
		 * Notify our controller that the job is about to spawn.  This
		 * is where ATTR_NUM_JOB_STARTS gets incremented for local
		 * universe jobs.
		 */
	virtual void notifyJobPreSpawn( void );

		/**
		 * A job is exiting the Starter and we need to take necessary
		 * actions. First we will update the job's ad file with various
		 * information about what the job did. Next, if the job completed on
		 * its own, we'll want to call the StarterUserPolicy's checkAtExit(),
		 * which handles writing any user log events and updating the job
		 * queue back on the schedd. If the job is being killed from "unnatural"
		 * causes, such as a condor_rm, then we will figure out the right
		 * update type is for the job and write an EVICT event to the user log.
		 * 
		 * @param exit_status - the exit status of the job from wait()
		 * @param reason - the Condor-defined reason why the job is exiting
		 * @param user_proc - the Proc object for this job
		 * @return true if the job was set to exit properly
		 * @see h/exit.h
		 **/
	virtual bool notifyJobExit( int exit_status, int reason, 
								UserProc* user_proc );  

	virtual bool notifyStarterError( const char* err_msg, bool critical, int hold_reason_code, int hold_reason_subcode );

protected:

	void setExitCodeToRequeue();

		/// This version confirms we're handling a "local" universe job. 
	virtual bool getUniverse( void );

		/// Initialize our local UserLog-writing code.
	virtual bool initLocalUserLog( void );

		/** Set a timer to call Starter::allJobsDone() so we retry
		    our attempts to cleanup the job, update the job queue, etc.
		*/
	void retryJobCleanup( void );

		/// DaemonCore timer handler to actually do the retry.
	void retryJobCleanupHandler( int timerID = -1 );

		/// Timer id for the job cleanup retry handler.
	int m_cleanup_retry_tid;

		/// Number of times we have retried job cleanup.
	int m_num_cleanup_retries;

		/// Maximum number of times we will retry job cleanup.
	int m_max_cleanup_retries;

		/// How long to delay between attempts to retry job cleanup.
	int m_cleanup_retry_delay;

		/// The value we will exit with to tell our schedd what happened
	int exit_code;

		/// The sinful string of the schedd's qmgmt command port
	char* schedd_addr;

		/// object for managing updates to the schedd's job queue for
		/// dynamic attributes in this job.
	QmgrJobUpdater* job_updater;
	
		/// The UserPolicy object wrapper for doing periodic and check
		/// at exit policy evaluations
	StarterUserPolicy *starter_user_policy;

	bool m_tried_notify_job_exit;
};

#endif /* _CONDOR_JIC_LOCAL_SCHEDD_H */
