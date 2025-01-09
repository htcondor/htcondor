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


#if !defined(_CONDOR_JOB_INFO_COMMUNICATOR_H)
#define _CONDOR_JOB_INFO_COMMUNICATOR_H

#include "condor_daemon_core.h"
#include "condor_classad.h"
#include "user_proc.h"
#include "local_user_log.h"
#include "condor_holdcodes.h"
#include "enum_utils.h"
#include "guidance.h"

#if HAVE_JOB_HOOKS
#include "StarterHookMgr.h"
#endif /* HAVE_JOB_HOOKS */

struct UnreadyReason {
	int hold_code = 0;
	int hold_subcode = 0;
	std::string message;
};

/** 
	This class is a base class for the various ways a starter can
	receive and send information about the underlying job.  For now,
	there are two main ways to do this: 1) to talk to a condor_shadow
	and 2) the local filesystem, command line args, etc.
*/

class JobInfoCommunicator : public Service {
public:
		/// Constructor
	JobInfoCommunicator();

		/// Destructor
	virtual ~JobInfoCommunicator();

		/// Pure virtual functions:

		// // // // // // // // // // // //
		// Initialization
		// // // // // // // // // // // //

		/** Initialize ourselves.  This should perform the following
			actions, no matter what kind of job controller we're
			dealing with:
			- aquire the job classad
			- call registerStarterInfo()
			- call initUserPriv()
			- call initJobInfo()
			@return true if we successfully initialized, false if we
			   failed and need to abort
		*/
	virtual bool init( void ) = 0;

		/// Read anything relevent from the config file
	virtual void config( void ) = 0;

		/// Setup the execution environment for the job.
		/// derived classes should implement this and call setupCompleted when done
	virtual void setupJobEnvironment( void ) = 0;
		// called by the above on completion or failure
		// status==0 is completion, other statuses can be JOB_SHOULD_HOLD, JOB_SHOULD_REQUEUE
	void setupCompleted(int status, const struct UnreadyReason * purea = nullptr);
		//const char * message=nullptr, int hold_code=0, int hold_subcode=0);

	void setStdin( const char* path );
	void setStdout( const char* path );
	void setStderr( const char* path );
	int getStackSize(void);	
		// // // // // // // // // // // //
		// Job Actions
		// // // // // // // // // // // //
		
		/**
		 * These are to perform specific actions against a job,
		 * which is different from methods such as gotHold() which
		 * are merely just a way for a Starter to tell us that a
		 * change occurred. These need to be implemented in the
		 * derived classes. Currently, on JICLocalSchedd only has
		 * any real logic to do something with these.
		 **/
	virtual bool holdJob( const char*, int hold_reason_code, int hold_reason_subcode ) = 0;
	virtual bool removeJob( const char* ) = 0;
	virtual bool terminateJob( const char* ) = 0;
	virtual bool requeueJob( const char* ) = 0;

		// // // // // // // // // // // //
		// Information about the job 
		// // // // // // // // // // // //
	
		/** Return a pointer to the filename to use for the job's
			standard input file.
		*/
	virtual const char* jobInputFilename( void );	

		/** Return a pointer to the filename to use for the job's
			standard output file.
		*/
	virtual const char* jobOutputFilename( void );	

		/** Return a pointer to the filename to use for the job's
			standard error file.
		*/
	virtual const char* jobErrorFilename( void );	

		/** Return a string containing a copy of the full pathname of
			the requested file.
		*/
	virtual char* getJobStdFile( const char* attr_name ) = 0;

	virtual bool streamInput();
	virtual bool streamOutput();
	virtual bool streamError();
	virtual bool streamStdFile( const char *which );

		/** Return a pointer to the job's initial working directory. 
		*/
	virtual const char* jobIWD( void );

		/** Returns a pointer to the job's remote initial working dir.
			This is the same as jobIWD() unless the job ad contains
			REMOTE_IWD.
		*/
	virtual const char* jobRemoteIWD( void );

		/** true if the starter is using a different iwd than the one
			in the job ad, false if not.
		*/
	virtual bool iwdIsChanged( void ) { return change_iwd; };

		/// Return a pointer to the original name for the job.
	virtual const char* origJobName( void );

		/// Return a pointer to the ClassAd for our job.
	virtual ClassAd* jobClassAd( void );

		/// Return a pointer to the execution overlay ClassAd for our job.
	virtual ClassAd* jobExecutionOverlayAd( void );

		/// Return a pointer to the ClassAd for the machine.
	virtual ClassAd* machClassAd( void );

		/// Return the job's universe integer.
	int jobUniverse( void ) const;

	virtual int jobCluster( void ) const;
	virtual int jobProc( void ) const;
	int jobSubproc( void ) const;

		/// Total bytes sent by this job 
	virtual uint64_t bytesSent( void ) = 0;

		/// Total bytes received by this job 
	virtual uint64_t bytesReceived( void ) = 0;


		// // // // // // // // // // // //
		// Job execution and state changes
		// // // // // // // // // // // //

		/**
		   All jobs have been spawned by the starter.

		   In the baseclass, this handles starting a timer for the
		   periodic job updates.  Subclasses might care about other
		   things at this point, too.
		 */
	virtual void allJobsSpawned( void );

		/** The starter has been asked to suspend.  Take whatever
			steps make sense for the JIC, and notify our job
			controller what happend.
		*/
	virtual void Suspend( void ) = 0;

		/** The starter has been asked to continue.  Take whatever
			steps make sense for the JIC, and notify our job
			controller what happend.
		*/
	virtual void Continue( void ) = 0;

		/** The last job this starter is controlling has exited.  Do
			whatever we have to do to cleanup and notify our
			controller. 
			@return true if it worked and the starter can continue
			cleaning up, false if there was an error (e.g. we're
			disconnected from our shadow) and the starter needs to go
			back to DaemonCore awaiting other events before it can
			finish the task...
		*/
	virtual bool allJobsDone( void );

		/**
		   Non-blocking API for allJobsDone().  This is called by the
		   handler for HOOK_JOB_EXIT and the timout handler so that the JIC knows the hook
		   is done and can resume the job cleanup process.
		*/
	void finishAllJobsDone( void );

		/** Once all the jobs are done, and after the optional
			HOOK_JOB_EXIT has returned, we need a step to handle
			internal file transfer for the output.  This only makes
			sense for JICShadow, but we need this step to be included
			in all JICs so that the code path during cleanup is sane.
			Returns false on failure and sets transient_failure to
			true if the failure is deemed transient and will therefore
			be automatically tried again (e.g. when the shadow reconnects).
		*/
	virtual bool transferOutput( bool &transient_failure ) = 0;
	virtual bool transferOutputMopUp( void ) = 0;
	void setJobFailed() { job_failed = true; }
	//bool getJobFailed() { return job_failed; }

		/** The last job this starter is controlling has been
			completely cleaned up.  Do whatever final work we want to
			do to shutdown, notify others, etc.
		*/
	virtual void allJobsGone( void ) = 0;

		/** The starter has been asked to shutdown fast.
		 */
	virtual void gotShutdownFast( void );

		/** The starter has been asked to shutdown gracefully.
		 */
	virtual void gotShutdownGraceful( void );

		/** The starter has been asked to evict for condor_rm
		 */
	virtual void gotRemove( void );

		/** The starter has been asked to evict for condor_hold
		 */
	virtual void gotHold( void );

	bool hadRemove( void ) const { return had_remove; };
	bool hadHold( void ) const { return had_hold; };
	bool isExiting( void ) const { return requested_exit; };
	bool isGracefulShutdown( void ) const { return graceful_exit; };
	bool isFastShutdown( void ) const { return fast_exit; };


		/** Someone is attempting to reconnect to this job.
		 */
	virtual int reconnect( ReliSock* s, ClassAd* ad ) = 0;

		/** Someone is attempting to disconnect from this job.
		 */
	virtual void disconnect() = 0;

		// // // // // // // // // // // //
		// Notfication to our controller
		// // // // // // // // // // // //

		/** Notify our controller that the job is about to spawn
		 */
	virtual void notifyJobPreSpawn( void ) = 0;

	// Notify our controller that one of the job's execution has completed.
	// This is technically redundant with notifyJobExit(), except that for
	// self-checkpointing jobs, the job can complete arbitrarily many
	// executions without exiting.  We also don't want to call
	// notifyJobTermination(), since that updates a bunch of other
	// attributes that we don't want to change.  (HTCONDOR-861)
	virtual void notifyExecutionExit( void ) { }


    #define GENERIC_EVENT_RV_OK          0
    // The event ad was missing an attribute expected for its type.
    #define GENERIC_EVENT_RV_INCOMPLETE -1
    // The event ad didn't include ATTR_EVENT_TYPE.
    #define GENERIC_EVENT_RV_NO_ETYPE   -2
    // The shadow didn't recognize the event type.
    #define GENERIC_EVENT_RV_UNKNOWN    -3
    // The shadow didn't like something about an attribute's value.
    #define GENERIC_EVENT_RV_CONFUSED   -4

    // Better than writing a bunch of tiny wrappers?
    virtual bool notifyGenericEvent( const ClassAd &, int & /* rv */ ) { return false; }


		/** Notify our controller that the job exited
			@param exit_status The exit status from wait()
			@param reason The Condor-defined exit reason
			@param user_proc The UserProc that was running the job
		*/
	virtual bool notifyJobExit( int exit_status, int reason,
								UserProc* user_proc ) = 0;

	// See jic_shadow.h for the explanation of how this differs from the
	// preceeding.  Only called if Starter::transferOutput() fails.
	virtual int notifyJobTermination( UserProc* user_proc ) = 0;

	virtual bool notifyStarterError( const char* err_msg, bool critical, int hold_reason_code, int hold_reason_subcode ) = 0;


	void setOutputAdFile( const char* path );
	const char* getOutputAdFile( void ) { return job_output_ad_file; };
	bool writeOutputAdFile( ClassAd* ad );
	void initOutputAdFile( void );

	void setUpdateAdFile( const char* path );
	const char* getUpdateAdFile( void ) { return m_job_update_ad_file.c_str(); };
	bool writeUpdateAdFile( ClassAd* ad );

	void setCredPath( const char* path );
	const char* getCredPath( void ) { return job_CredPath; };

	void setKrb5CCName( const char* path );
	const char* getKrb5CCName( void ) { return job_Krb5CCName; };

		/**
		   Send a periodic update ClassAd to our controller.
		   @param update_ad Update ad to use if you've already got the info
		   @return true if success, false if failure
		*/
	virtual bool periodicJobUpdate(ClassAd* update_ad = NULL);

		/**
		   Function to be called periodically to update the controller.
		   We can't just register a timer to call periodicJobUpdate()
		   directly, since DaemonCore isn't passing any args to timer
		   handlers, and therefore, it doesn't know it's supposed to
		   honor the default arguments.  So, we use this seperate
		   function to register for the periodic 
		   updates, and this ensures that we use the UDP version of
		   UpdateShadow().
		*/
	void periodicJobUpdateTimerHandler( int timerID = -1 );

		/**
		   Return the max controller update interval in seconds, or -1 if unknown.
		*/
	int periodicJobUpdateTimerMaxInterval( void );

		// // // // // // // // // // // //
		// Misc utilities
		// // // // // // // // // // // //

		/** Make sure the given filename will be included in the
			output files of the job that are sent back to the job
			submitter.  
			@param filename File to add to the job's output list 
		*/
	virtual void addToOutputFiles( const char* filename ) = 0;

		/** Make sure the given filename will be excluded from the
			list of files that the job sends back to the submitter.  
			@param filename File to remove from the job's output list 
		*/
	virtual void removeFromOutputFiles( const char* filename ) = 0;

		/// Has user_priv been initialized yet?
	bool userPrivInitialized( void ) const; 

		/** Are we currently using file transfer? 
		    Used elsewhere to determine if we need to potentially
			rewrite some paths from submit machine paths to
			execute directory paths.

			The default implementation always returns false.
			jic_shadow actually comes up with a plausible answer. 
		*/
	virtual bool usingFileTransfer( void );

		/* Receive new X509 proxy from the shadow
			
			Default implementation always fails.

			jic_shadow knows how to do the right thing.
		 */
	virtual bool updateX509Proxy( int cmd, ReliSock * s );

		/** Return whether or not the uid chosen by initUserPriv() is
			only for use by this job, so we can track the processes
			spawned by the job via uid.  Returns NULL if account is
			not dedicated.  Otherwise, returns name of account.
		 */
	char const *getExecuteAccountIsDedicated( ) {
		return m_dedicated_execute_account;
	}

		/* Upload files in a job working directory */
	virtual bool uploadWorkingFiles(void) { return false; }

	virtual bool uploadCheckpointFiles( int /* checkpointNumber */ ) { return false; }

		/* Update Job ClassAd with checkpoint info and log it */
	virtual void updateCkptInfo(void) {};

		/* Methods to get chirp config information */
	virtual bool wroteChirpConfig() { return false; }
	virtual const std::string chirpConfigFilename() { return ""; }

		/* Get the job ad */
	const ClassAd * getJobAd() { return job_ad; }

	virtual bool genericRequestGuidance(
		const ClassAd & /* request */, GuidanceResult & /* rv */, ClassAd & /* guidance */
	) {
		return false;
	}


protected:

		// // // // // // // // // // // //
		// Protected helper methods
		// // // // // // // // // // // //

		/** Register some important information about ourself that the
			job controller might needs.
			@return true on success, false on failure
		*/
	virtual	bool registerStarterInfo( void ) = 0;

		/** Initialize the priv_state code with the appropriate user
			for this job.
			@return true on success, false on failure
		*/
	virtual bool initUserPriv( void ) = 0;

		/** Get the configuration (plus job) policy for whether to
			try running the job as the owner of the job.  Unfortunately,
			windows and unix have different defaults, so these are
			taken as arguments to this function.  This function only
			returns True if RunAsOwner is allowed by the starter and
			requested by the job, (substituting in the default for
			either of those if they are not specified).
		 */
	bool allowRunAsOwner( bool default_allow, bool default_request );

		/** See if the specified name matches the list of dedicated
			login accounts for executing jobs.
		 */
	bool checkDedicatedExecuteAccounts( char const *name );

		/** This may be called by initUserPriv() to specify that the
			chosen uid is only for use by this job, so we can track
			the processes spawned by the job via uid.  Pass NULL
			if account is not dedicated.  Otherwise, pass the name
			of the dedicated account.
		*/
	void setExecuteAccountIsDedicated( char const *name );

		/** Initialize the priv_state code on Windows.  For now, this
			is identical code, no matter what kind of JIC we're using,
			so put it in 1 place to avoid duplication
		*/
	bool initUserPrivWindows( void );

		/** See if we can initialize user_priv without ATTR_OWNER.  if
			we can, do it and return true.  If not, return false.
		*/
	bool initUserPrivNoOwner( void ); 

		/** Publish information into the given classad for updates to
			our job controller
			@param ad ClassAd pointer to publish into
			@return true if success, false if failure
		*/ 
	virtual bool publishUpdateAd( ClassAd* ad ) = 0;

		/** Initialize our version of important information for this
			job which the starter will want to know.  This should
			init the following: orig_job_name, job_input_name, 
			job_output_name, job_error_name, job_iwd, 
			job_universe, job_cluster, job_proc, and job_subproc.
			The base class also looks up the hook keyword from the job
			ClassAd (if any) and instantiates/initializes the HookMgr.
			@return true on success, false on failure */
	virtual	bool initJobInfo( void );

		/** Since we want to support the ATTR_STARTER_WAIT_FOR_DEBUG,
			as soon as we have the job ad, each JIC subclass will want
			to do this work at a different time.  However, since the
			code is the same in all cases, we use this helper in the
			base class to do the work, which looks up the attr in the
			job ad, and if it's defined as true, we go into the
			infinite loop, waiting for someone to attach with a
			debugger.  This also handles printing out the job classad
			to D_JOB if that's in our DebugFlags.
		 */
	virtual void checkForStarterDebugging( void );

		/** Helper for dumping a copy of the job ad to the sandbox.
		*/
	virtual void writeExecutionVisa( ClassAd& );


		// // // // // // // // // // // //
		// Protected data members
		// // // // // // // // // // // //

	LocalUserLog* u_log;

		/** The real job executable name (after ATTR_JOB_CMD
			is switched to condor_exec).
		*/
	char* orig_job_name;

	char* job_input_name;

	char* job_output_name;

	char* job_error_name;

	char* job_iwd;

	char* job_remote_iwd;

	char* job_output_ad_file;
	bool job_output_ad_is_stdout;

	std::string m_job_update_ad_file;

	char* job_CredPath;
	char* job_Krb5CCName;
	
		/// The ClassAd for our job.  We control the memory for this.
	ClassAd* job_ad;

		/// The execution overlay ClassAd for our job.  We control the memory for this.
	ClassAd* job_execution_overlay_ad;

		// The Machine ClassAd running the job.
	ClassAd* mach_ad;

		/// The universe of the job.
	int job_universe;

	int job_cluster;
	int	job_proc;
	int	job_subproc;

		/// if true, we were asked to shutdown
	bool requested_exit;
	bool graceful_exit;
	bool fast_exit;
	bool had_remove;
	bool had_hold;
	bool job_failed=false;

		/** true if we're using a different iwd for the job than what
			the job ad says.
		*/
	bool change_iwd;

	bool user_priv_is_initialized;

	std::string m_dedicated_execute_account_buf;
	char const *m_dedicated_execute_account;

#if HAVE_JOB_HOOKS
	StarterHookMgr* m_hook_mgr;
#endif /* HAVE_JOB_HOOKS */

	bool m_enforce_limits;

private:
		/// Start a timer for the periodic job updates
	void startUpdateTimer( void );

		/// Cancel our timer for the periodic job updates
	void cancelUpdateTimer( void );


		/// timer id for periodically sending info on job to Shadow
	int m_periodic_job_update_tid;

	bool m_allJobsDone_finished;

		/**
		   @return The exit reason string representing what happened to
		     the job.  Possible values: "exit" (on its own), "hold",
		     "remove", or "evict" (PREEMPT, condor_vacate, condor_off).
		*/
	const char* getExitReasonString( void ) const;

#if HAVE_JOB_HOOKS
	// timer handler for timing out a JobExit hooks, calls finishJobsDone
	int m_exit_hook_timer_tid = -1;
	void hookJobExitTimedOut( int timerID = -1 );
#endif
};


#endif /* _CONDOR_JOB_INFO_COMMUNICATOR_H */
