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

#if !defined(_CONDOR_STARTER_H)
#define _CONDOR_STARTER_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "list.h"
#include "user_proc.h"
#include "job_info_communicator.h"


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
	virtual void StarterExit( int code );

		/** Params for "EXECUTE" and other useful stuff 
		 */
	virtual void Config();

	virtual int SpawnJob( void );

		/** Walk through list of jobs, call ShutDownGraceful on each.
			@return 1 if no jobs running, 0 otherwise 
		*/
	virtual int ShutdownGraceful(int);

		/** Walk through list of jobs, call ShutDownFast on each.
			@return 1 if no jobs running, 0 otherwise 
		*/
	virtual int ShutdownFast(int);

		/** Create the execute/dir_<pid> directory and chdir() into
			it.  This can only be called once user_priv is initialized
			by the JobInfoCommunicator.
		*/
	virtual bool createTempExecuteDir( void );

		/** Called by the JobInfoCommunicator whenever the job
			execution environment is ready so we can actually spawn
			the job.
		*/
	virtual int jobEnvironmentReady( void );

		/** Does final cleanup once all the jobs (and post script, if
			any) have completed.  This deals with everything on the
			CleanedUpJobList, notifies the JIC, etc.
		*/
	virtual void allJobsDone( void );

		/** Call Suspend() on all elements in JobList */
	virtual int Suspend(int);

		/** Call Continue() on all elements in JobList */
	virtual int Continue(int);

		/** To do */
	virtual int PeriodicCkpt(int);

		/** Call Remove() on all elements in JobList */
	virtual int Remove(int);

		/** Call Hold() on all elements in JobList */
	virtual int Hold(int);

		/** Handles the exiting of user jobs.  If we're shutting down
			and there are no jobs left alive, we exit ourselves.
			@param pid The pid that died.
			@param exit_status The exit status of the dead pid
		*/
	virtual int Reaper(int pid, int exit_status);

		/** Return the Working dir */
	const char *GetWorkingDir() const { return WorkingDir; }

		/** Publish all attributes we care about for our job
			controller into the given ClassAd.  Walk through all our
			UserProcs and have them publish.
            @param ad pointer to the classad to publish into
			@return true if we published any info, false if not.
		*/
	bool publishUpdateAd( ClassAd* ad );

	bool publishPreScriptUpdateAd( ClassAd* ad );
	bool publishPostScriptUpdateAd( ClassAd* ad );

		/** Put all the environment variables we'd want a Proc to have
			into the given Env object.  This will figure out what Proc
			objects we've got and will call their respective
			PublishToEnv() methods
			@param proc_env The environment to publish to
		*/
	void PublishToEnv( Env* proc_env );
	
		/** Pointer to our JobInfoCommuniator object, which abstracts
			away any details about our communications with whatever
			entity is controlling our job.  This way, the starter can
			easily run without talking to a shadow, by instantiating a
			different kind of jic.  We want this public, since lots of
			parts of the starter need to get to this thing, and it's
			just easier this way. :)
		*/
	JobInfoCommunicator* jic;

		/** Returns the VM number we're running on, or 0 if we're not
			running as a VM at all.
		*/
	int getMyVMNumber( void );

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

protected:
	List<UserProc> JobList;
	List<UserProc> CleanedUpJobList;

private:

		// // // // // // // //
		// Private Methods
		// // // // // // // //

		/// Remove the execute/dir_<pid> directory
	virtual bool removeTempExecuteDir( void );


		// // // // // // // //
		// Private Data Members
		// // // // // // // //

	int jobUniverse;

	char *Execute;
	char WorkingDir[_POSIX_PATH_MAX]; // The iwd given to the job
	char ExecuteDir[_POSIX_PATH_MAX]; // The scratch dir created for the job
	char *orig_cwd;
	bool is_gridshell;
	int ShuttingDown;
	int starter_stdin_fd;
	int starter_stdout_fd;
	int starter_stderr_fd;

	UserProc* pre_script;
	UserProc* post_script;
};

#endif


