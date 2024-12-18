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
#include "user_proc.h"
#include "job_info_communicator.h"
#include "execute_dir_monitor.h"
#include "exit.h"

#if defined(LINUX)
#include "../condor_startd.V6/VolumeManager.h"
#endif

#ifdef WIN32
#include "profile.WINDOWS.h" // for OwnerProfile class
#endif

#if 1 //def HAVE_DATA_REUSE_DIR TOO: remove this someday
namespace htcondor {
class DataReuseDirectory;
}
#endif

/** The starter class.  Basically, this class does some initialization
	stuff and manages a set of UserProc instances, each of which 
	represent a running job.

	@see UserProc
 */
class Starter : public Service
{
public:
		/// Constructor
	Starter();
		/// Destructor
	virtual ~Starter();

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
	virtual void PREFAST_NORETURN StarterExit( int code ) GCC_NORETURN;

		/** Do any potential cleanup before exiting. Used both in 
		    successful exits (StarterExit()) and EXCEPT()ions.
			Do not call this function lightly!

			It is conceivable that this function will be called
			several times if, say, an EXCEPT()ion happens while
			shutting down, so be careful in the implementation.
		*/
	virtual int FinalCleanup(int code);

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

	virtual int jobEnvironmentCannotReady(int status, const struct UnreadyReason & urea);

	// Timer call-backs, so that if we want a non-blocking guidance protocol,
	// we can easily implement one (with a coroutine).
	void requestGuidanceJobEnvironmentReady( int /* timerID */ );
	void requestGuidanceJobEnvironmentUnready( int /* timerID */ );

		/**
		 *
		 *
		 **/
	virtual void SpawnPreScript( int timerID = -1 );

		/* timer to handle unwinding to pump while skipping job spawn */
	virtual void SkipJobs( int timerID = -1 );

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

		/** Return the base Execute directory for this slot */
	const char *GetExecuteDir() const { return Execute; }

		/** Return the temporary directory under Execute for this job.
		 *  If file transfer is used, this will also be the job's IWD.
		 */
	const char *GetWorkingDir(bool inner) const {
		if (inner && ! InnerWorkingDir.empty()) {
			return InnerWorkingDir.c_str();
		}
		return WorkingDir.c_str();
	}
		/* Should the temporary directory under Execute be expected to
		 * exist?
		 */
	bool WorkingDirExists() const { return m_workingDirExists; }
		/* Set the working dir from the perspective of the job. This may differ from
		*  the Starter's WorkingDir value when the job is in a container.  For a containerized job
		*  Working dir will be something like /var/lib/condor/execute/dir_nnnn  or C:\Condor\Execute\dir_nnnn
		*  while inner working dir might be  /scratch/condor_job on all platforms
		*/
	void SetInnerWorkingDir(const char * inner_dir) {
		if (inner_dir) {
			InnerWorkingDir = inner_dir;
		} else {
			InnerWorkingDir.clear();
		}
	}

	// Get job working directory disk usage: return bytes used & num dirs + files
	DiskUsage GetDiskUsage(bool exiting=false) const;

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

		/** Open a file in the 'manifest' directory.
		 */
	FILE * OpenManifestFile(const char * filename);

		/** Set up the complete environment for the job.  This includes
			STARTER_JOB_ENVIRONMENT, the job ClassAd, and PublishToEnv()
		*/
	bool GetJobEnv( ClassAd *jobad, Env *job_env, std::string & env_errors );

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
	std::string getMySlotName( void );

		/** Returns the number of jobs currently running under
		 * this multi-starter.
		 */
	int numberOfJobs( void ) { return m_job_list.size(); };

	bool isGridshell( void ) const {return is_gridshell;};

	bool hasEncryptedWorkingDir(void) { return has_encrypted_working_dir; }
#ifdef WIN32
	bool loadUserRegistry(const ClassAd * jobAd);
#endif
	const char* origCwd( void ) {return (const char*) orig_cwd;};
	int starterStdinFd( void ) const { return starter_stdin_fd; };
	int starterStdoutFd( void ) const { return starter_stdout_fd; };
	int starterStderrFd( void ) const { return starter_stderr_fd; };
	void closeSavedStdin( void );
	void closeSavedStdout( void );
	void closeSavedStderr( void );

		/** Command handler for ClassAd-only protocol commands */
	int classadCommand( int, Stream* ) const;

	int updateX509Proxy( int cmd, Stream* );

	int createJobOwnerSecSession( int cmd, Stream* s );

	int startSSHD( int /*cmd*/, Stream* s );	
	int SSHDFailed(Stream *s,char const *fmt,...) CHECK_PRINTF_FORMAT(3,4);
	int SSHDRetry(Stream *s,char const *fmt,...) CHECK_PRINTF_FORMAT(3,4);
	int vMessageFailed(Stream *s,bool retry, const std::string &, char const *fmt,va_list args);

	int peek( int /*cmd*/, Stream* s );
	int PeekFailed(Stream *s,char const *fmt,...) CHECK_PRINTF_FORMAT(3,4);
	int PeekRetry(Stream *s,char const *fmt,...) CHECK_PRINTF_FORMAT(3,4);


	int GetShutdownExitCode() const { return m_shutdown_exit_code; };
	void SetShutdownExitCode( int code ) { m_shutdown_exit_code = code; };

#ifdef HAVE_DATA_REUSE_DIR
	htcondor::DataReuseDirectory * getDataReuseDirectory() const {return m_reuse_dir.get();}
#else
	htcondor::DataReuseDirectory * getDataReuseDirectory() const {return nullptr;}
#endif

	void SetJobEnvironmentReady(const bool isReady) {m_job_environment_is_ready = isReady;}

	virtual void RecordJobExitStatus(int status);

	void setTmpDir(const std::string &dir) { this->tmpdir = dir;}
protected:
	std::vector<UserProc *> m_job_list;
	std::vector<UserProc *> m_reaped_job_list;

	// Code shared by the requestGuidance...() functions.
	bool handleJobEnvironmentCommand(
		const ClassAd & guidance,
		std::function<void(int)> continue_conversation
	);

	// JobEnvironmentCannotReady sets these to pass along the setup failure info that
	// we want to report *after* we finish transfer of FailureFiles
	int            m_setupStatus = 0; // 0 is success, non-zero indicates failure of job setup
	struct UnreadyReason  m_urea; // details when m_setupStatus is non-zero

#ifdef WIN32
	OwnerProfile m_owner_profile;
#endif

	bool m_deferred_job_update;

	bool recorded_job_exit_status{false};
	int job_exit_status;
private:

		// // // // // // // //
		// Private Methods
		// // // // // // // //

		/// Remove the execute/dir_<pid> directory
		/// Argument exit_code: override Starter exit code with value
	virtual bool removeTempExecuteDir(int& exit_code);

		/**
		   Iterate through a UserProc list and have each UserProc
		   publish itself to the given ClassAd.

		   @param proc_list List of UserProc objects to iterate.
		   @param ad ClassAd to publish info into.

		   @return true if we published anything, otherwise false.

		   @see Starter::publishUpdateAd()
		   @see Starter::publishJobExitAd()
		   @see UserProc::PublishUpdateAd()
		*/
	bool publishJobInfoAd(std::vector<UserProc * > *proc_list, ClassAd* ad);

		/*
		  @param result Buffer in which to store fully-qualified user name of the job owner
		  If no job owner can be found, substitute a suitable dummy user name.
		 */
	void getJobOwnerFQUOrDummy(std::string &result) const;

		/*
		  @param result Buffer in which to store claim id string from job.
		  Returns false if no claim id could be found.
		 */
	bool getJobClaimId(std::string &result) const;


	bool WriteAdFiles() const;

		// // // // // // // //
		// Private Data Members
		// // // // // // // //

	// Monitor for job working dir for disk usage
	ExecDirMonitor* dirMonitor;

	int jobUniverse;

		// The base EXECUTE directory for this slot
	char *Execute;
		// The temporary directory created under Execute for this job.
		// If file transfer is used, this will also be the IWD of the job.
	std::string WorkingDir;
	std::string InnerWorkingDir; // if non-empty, this is the jobs view if the working dir
	char *orig_cwd;
	std::string m_recoveryFile;
	bool is_gridshell;
	bool m_workingDirExists;
	bool has_encrypted_working_dir;

	int ShuttingDown;
	int starter_stdin_fd;
	int starter_stdout_fd;
	int starter_stderr_fd;

	std::string m_vacateReason;
	int m_vacateCode{0};
	int m_vacateSubcode{0};

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

		//
		// When HTCondor manages dedicated disk space, this tracks
		// the maximum permitted disk usage and the polling timer
		//
#ifdef LINUX
	void CheckLVUsage( int timerID = -1 );
	std::unique_ptr<VolumeManager::Handle> m_lv_handle;
	int64_t m_lvm_lv_size_kb{0};
	time_t m_lvm_last_space_issue{-1};
	int m_lvm_poll_tid{-1};
	bool m_lvm_held_job{false};
#endif /* LINUX */

	UserProc* pre_script;
	UserProc* post_script;

		//
		// Flag to indicate whether Config() has been run
		//
	bool m_configured;

		// true if jobEnvironmentReady() has been called
	bool m_job_environment_is_ready;

		// true if allJobsDone() has been called
	bool m_all_jobs_done;

		// When doing a ShutdownFast or ShutdownGraceful, what should the
		// starter's exit code be?
	int m_shutdown_exit_code;

#ifdef HAVE_DATA_REUSE_DIR
	// Manage the data reuse directory.
	std::unique_ptr<htcondor::DataReuseDirectory> m_reuse_dir;
#endif

	// The string to set the tmp env vars to
	std::string tmpdir;
};

#define SANDBOX_STARTER_LOG_FILENAME ".starter.log"

#endif


