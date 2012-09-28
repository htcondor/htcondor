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


#if !defined(_CONDOR_STARTER_H)
#define _CONDOR_STARTER_H

#include "condor_daemon_core.h"
#include "list.h"
#include "user_proc.h"
#include "job_info_communicator.h"
#include "condor_privsep_helper.h"

#if defined(LINUX)
#include "glexec_privsep_helper.linux.h"
#endif

/** The starter class.  Basically, this class does some initialization
	stuff and manages a set of UserProc instances, each of which 
	represent a running job.

	@see UserProc
 */
class CStarter : public Service
{
public:
		/// Constructor
	CStarter();
		/// Destructor
	virtual ~CStarter();

		/** This is called at the end of main_init().  It calls
			Config(), registers a bunch of signals, registers a
			reaper, makes the starter's working dir and moves there,
			sets resource limits, then calls StartJob()
		*/
	virtual bool Init( JobInfoCommunicator* my_jic, 
					   const char* orig_cwd, bool is_gridshell,
					   int stdin_fd, int stdout_fd, int stderr_fd );

		/** The starter is finally ready to exit, so handle some
			cleanup code we always need, then call DC_Exit() with the
			given exit code.
		*/
	virtual void PREFAST_NORETURN StarterExit( int code );

		/** Do any potential cleanup before exiting. Used both in 
		    successful exits (StarterExit()) and EXCEPT()ions.
			Do not call this function lightly!

			It is conceivable that this function will be called
			several times if, say, an EXCEPT()ion happens while
			shutting down, so be careful in the implementation.
		*/
	virtual void FinalCleanup();

		/** Params for "EXECUTE" and other useful stuff 
		 */
	virtual void Config();

	virtual int SpawnJob( void );

	virtual void WriteRecoveryFile( ClassAd *recovery_ad );
	virtual void RemoveRecoveryFile();

		/*************************************************************
		 * Starter Commands
		 * We now have two versions of the old commands that the Starter
		 * registers with daemon core. The "Remote" version of the 
		 * function is what will registered. These functions will
		 * first notify the JIC that an action is occuring then call
		 * the regular version of the function. The reason why we do this
		 * now is because we need to be able to do things internally
		 * without the JIC thinking it was told from the outside
		 *************************************************************/

		/** Call Suspend() on all elements in m_job_list */
	virtual int RemoteSuspend( int );
	virtual bool Suspend( void );

		/** Call Continue() on all elements in m_job_list */
	virtual int RemoteContinue( int );
	virtual bool Continue( void );

		/** Call Ckpt() on all elements in m_job_list */
	virtual int RemotePeriodicCkpt( int );
	virtual bool PeriodicCkpt( void );

		/** Call Remove() on all elements in m_job_list */
	virtual int RemoteRemove( int );
	virtual bool Remove( void );

		/** Call Hold() on all elements in m_job_list */
	int remoteHoldCommand( int cmd, Stream* s );
	virtual int RemoteHold(int);
	virtual bool Hold( void );

		/** Walk through list of jobs, call ShutDownGraceful on each.
			@return 1 if no jobs running, 0 otherwise 
		*/
	virtual int RemoteShutdownGraceful( int );
	virtual bool ShutdownGraceful( void );

		/** Walk through list of jobs, call ShutDownFast on each.
			@return 1 if no jobs running, 0 otherwise 
		*/
	virtual int RemoteShutdownFast( int );
	virtual bool ShutdownFast( void );

		/** Create the execute/dir_<pid> directory and chdir() into
			it.  This can only be called once user_priv is initialized
			by the JobInfoCommunicator.
		*/
	virtual bool createTempExecuteDir( void );
	
		/**
		 * Before a job is spawned, this method checks whether
		 * a job has a deferrral time, which means we will need
		 * to register timer to call SpawnPreScript()
		 * when it is the correct time to run the job
		 */
	virtual bool jobWaitUntilExecuteTime( void );
	
		/**
		 * Clean up any the timer that we have might
		 * have registered to put a job on hold. As of now
		 * there can only be one job on hold
		 */
	virtual bool removeDeferredJobs( void );
		
		/** Called by the JobInfoCommunicator whenever the job
			execution environment is ready so we can actually spawn
			the job.
		*/
	virtual int jobEnvironmentReady( void );
	
		/**
		 * 
		 * 
		 **/
	virtual void SpawnPreScript( void );

		/** Does initial cleanup once all the jobs (and post script, if
			any) have completed.  This notifies the JIC so it can
			initiate HOOK_JOB_EXIT, file transfer, etc.
		*/
	virtual bool allJobsDone( void );

		/** Handles the exiting of user jobs.  If we're shutting down
			and there are no jobs left alive, we exit ourselves.
			@param pid The pid that died.
			@param exit_status The exit status of the dead pid
		*/
	virtual int Reaper(int pid, int exit_status);

		/** Called after HOOK_JOB_EXIT returns (if defined), when
			we're ready to transfer output.  This only initiates a
			file transfer in the case of talking to a shadow, but it
			helps keep the job exit code path sane to have this
			function at this level of the code.  This basically just
			turns around to invoke JIC::transferOutput(), but if that
			fails, we assume we're disconnected and return to
			DaemonCore, whereas if it succeeds, we know we're done
			with the file transfer and can call cleanupJobs().
		*/
	virtual bool transferOutput( void );

		/** Called after allJobsDone() and friends have finished doing
			all of their work (invoking HOOK_JOB_EXIT, file transfer,
			etc) when we're ready for the final cleanup of the jobs.
			This iterates over all of the UserProc objects and invokes
			JobExit() on each of them, removes them from the list, and
			once everything is cleaned, calls JIC::allJobsGone().
		*/
	virtual bool cleanupJobs( void );

		/** Return the Execute dir */
	const char *GetExecuteDir() const { return Execute; }

		/** Return the Working dir */
	const char *GetWorkingDir() const { return WorkingDir.Value(); }

		/** Publish all attributes we care about for our job
			controller into the given ClassAd.  Walk through all our
			UserProcs and have them publish.
            @param ad pointer to the classad to publish into
			@return true if we published any info, false if not.
		*/
	bool publishUpdateAd( ClassAd* ad );

	bool publishPreScriptUpdateAd( ClassAd* ad );
	bool publishPostScriptUpdateAd( ClassAd* ad );

		/**
		   Publish all attributes once the jobs have exited into the
		   given ClassAd.  Walk through all the reaped UserProc
		   objects and have them publish.
		   @param ad pointer to the classad to publish into
		   @return true if we published any info, false if not.
		*/
	bool publishJobExitAd( ClassAd* ad );

		/** Put all the environment variables we'd want a Proc to have
			into the given Env object.  This will figure out what Proc
			objects we've got and will call their respective
			PublishToEnv() methods
			@param proc_env The environment to publish to
		*/
	void PublishToEnv( Env* proc_env );

		/** Set up the complete environment for the job.  This includes
			STARTER_JOB_ENVIRONMENT, the job ClassAd, and PublishToEnv()
		*/
	bool GetJobEnv( ClassAd *jobad, Env *job_env, MyString *env_errors );
	
		/** Pointer to our JobInfoCommuniator object, which abstracts
			away any details about our communications with whatever
			entity is controlling our job.  This way, the starter can
			easily run without talking to a shadow, by instantiating a
			different kind of jic.  We want this public, since lots of
			parts of the starter need to get to this thing, and it's
			just easier this way. :)
		*/
	JobInfoCommunicator* jic;

		/** Returns the slot number we're running on, or 0 if we're not
			running on a slot at all.
		*/
	int getMySlotNumber( void );
	MyString getMySlotName( void );

		/** Returns the number of jobs currently running under
		 * this multi-starter.
		 */
	int numberOfJobs( void ) { return m_job_list.Number(); };

	bool isGridshell( void ) {return is_gridshell;};
	const char* origCwd( void ) {return (const char*) orig_cwd;};
	int starterStdinFd( void ) { return starter_stdin_fd; };
	int starterStdoutFd( void ) { return starter_stdout_fd; };
	int starterStderrFd( void ) { return starter_stderr_fd; };
	void closeSavedStdin( void );
	void closeSavedStdout( void );
	void closeSavedStderr( void );

		/** Command handler for ClassAd-only protocol commands */
	int classadCommand( int, Stream* );

	int updateX509Proxy( int cmd, Stream* );

	int createJobOwnerSecSession( int cmd, Stream* s );

	int startSSHD( int /*cmd*/, Stream* s );
	int SSHDFailed(Stream *s,char const *fmt,...) CHECK_PRINTF_FORMAT(3,4);
	int SSHDRetry(Stream *s,char const *fmt,...) CHECK_PRINTF_FORMAT(3,4);
	int vSSHDFailed(Stream *s,bool retry,char const *fmt,va_list args);

		/** This will return NULL if we're not using either
		    PrivSep or GLExec */
	PrivSepHelper* privSepHelper()
	{
		return m_privsep_helper;
	}
		/** This will return NULL if we're not using PrivSep */
	CondorPrivSepHelper* condorPrivSepHelper()
	{
		return dynamic_cast<CondorPrivSepHelper*>(m_privsep_helper);
	}
#if defined(LINUX)
	GLExecPrivSepHelper* glexecPrivSepHelper()
	{
		return dynamic_cast<GLExecPrivSepHelper*>(m_privsep_helper);
	}
#endif

protected:
	List<UserProc> m_job_list;
	List<UserProc> m_reaped_job_list;

	bool m_deferred_job_update;
private:

		// // // // // // // //
		// Private Methods
		// // // // // // // //

		/// Remove the execute/dir_<pid> directory
	virtual bool removeTempExecuteDir( void );

#if !defined(WIN32)
		/// Special cleanup for exiting after being invoked via glexec
	void exitAfterGlexec( int code );
#endif

		/**
		   Iterate through a UserProc list and have each UserProc
		   publish itself to the given ClassAd.

		   @param proc_list List of UserProc objects to iterate.
		   @param ad ClassAd to publish info into.

		   @return true if we published anything, otherwise false.

		   @see CStarter::publishUpdateAd()
		   @see CStarter::publishJobExitAd()
		   @see UserProc::PublishUpdateAd()
		*/
	bool publishJobInfoAd(List<UserProc>* proc_list, ClassAd* ad);

		/*
		  @param result Buffer in which to store fully-qualified user name of the job owner
		  If no job owner can be found, substitute a suitable dummy user name.
		 */
	void getJobOwnerFQUOrDummy(MyString &result);

		/*
		  @param result Buffer in which to store claim id string from job.
		  Returns false if no claim id could be found.
		 */
	bool getJobClaimId(MyString &result);


	bool WriteAdFiles();
		// // // // // // // //
		// Private Data Members
		// // // // // // // //

	int jobUniverse;

	char *Execute;
	MyString WorkingDir; // The iwd given to the job
	char *orig_cwd;
	MyString m_recoveryFile;
	bool is_gridshell;
	int ShuttingDown;
	int starter_stdin_fd;
	int starter_stdout_fd;
	int starter_stderr_fd;
	
		//
		// When set to true, that means the Starter was asked to
		// suspend all jobs. This is used when jobs are started up
		// after the Suspend call came in so that we don't start
		// jobs when we shouldn't
		//
	bool suspended;
	
		//
		// This is the id of the timer for when a job gets deferred
		//
	int deferral_tid;

	UserProc* pre_script;
	UserProc* post_script;

		//
		// If we're using PrivSep, we'll need a helper object to
		// manage sandbox ownership and to launch the job.
		//
	PrivSepHelper* m_privsep_helper;

		//
		// Flag to indicate whether Config() has been run
		//
	bool m_configured;

		// true if jobEnvironmentReady() has been called
	bool m_job_environment_is_ready;

		// true if allJobsDone() has been called
	bool m_all_jobs_done;
};

#endif


