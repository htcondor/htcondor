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

#if !defined(_CONDOR_USER_PROC_H)
#define _CONDOR_USER_PROC_H

#include "condor_daemon_core.h"
#include "condor_distribution.h"
#include "utc_time.h"
#include "env.h"
#include "condor_classad.h"

/** This class is a base class for the various types of startable
	processes.  It defines a bunch of pure virtual functions that 
	are to be implemented in child classes.

 */
class UserProc : public Service
{
public:
		/// Constructor
	UserProc() : JobAd(nullptr), name(nullptr) { initialize(); }; 

		/// Destructor
	virtual ~UserProc();

		/** Pure virtual functions: */
			//@{

		/** Start this job.  Starter should delete this object if 
			StartJob returns 0.
			@return 1 on success, 0 on failure.
		*/
	virtual int StartJob() = 0;

		/** A pid exited.  If this UserProc wants to do any cleanup
			now that this pid has exited, it does so here.  If we
			return true, the starter will consider this UserProc done,
			remove it from the active job list, and put it in a list
			of jobs that are already reaped.
		    @return true if this UserProc is no longer active, false if it is
		*/
	virtual bool JobReaper(int pid, int status);

		/** Job exits.  Starter has decided it's done with everything
			it needs to do, and we can now notify the job's controller
			we've exited so it can do whatever it wants to.
		    @return true on success, false on failure
		*/
	virtual bool JobExit( void ) = 0;

		/** Publish all attributes we care about for updating the
			job controller into the given ClassAd.
			@param ad pointer to the classad to publish into
			@return true if success, false if failure
		*/
	virtual bool PublishUpdateAd( ClassAd* ad );

		/** Put all the environment variables we'd want for other
			procs into the given Env object.
			@param proc_env The environment to publish to
		*/
	virtual void PublishToEnv( Env* proc_env );

		/** Suspend. */
	virtual void Suspend() = 0;

		/** Continue. */
	virtual void Continue() = 0;

	virtual bool Remove() = 0;

	virtual bool Hold() = 0;

		/** Graceful shutdown, aka soft kill. 
			@return true if shutdown complete, false if pending */
	virtual bool ShutdownGraceful() = 0;

		/** Fast shutdown, aka hard kill. 
			@return true if shutdown complete, false if pending */
	virtual bool ShutdownFast() = 0;
		//@}

		/** Checkpoint */
	virtual bool Ckpt() { return false; }
	virtual void CkptDone(bool /*success*/) {};

		/** Returns the pid of this job.
			@return The pid. */
	int GetJobPid() const { return JobPid; }

		/** Check if user's job process has actually been started yet.
			For instance, it may not have been forked yet because we're
			waiting for data files to be transfered.
			@return true if job has been started, false if not. */
	bool JobStarted() const { return JobPid > 0; }

		/** Was this job requested to exit by the starter, or did it 
			exit on its own?
		*/
	bool RequestedExit( void ) const { return requested_exit; };

		/** This is the login id used for execute login is dedicated pid tracking.
		 *  If there is no dedicated login for job execution, this is NULL.
		 *  (public so it can be seen by derived grandchildren classes; protected
		 *   only allow derived child classes access, not grandchildren).
		 */
	char const *m_dedicated_account;

protected:

	void initialize( void );

	ClassAd *JobAd;
	int JobPid;
	int job_universe;
	int exit_status;
	bool requested_exit;
	bool m_proc_exited;

		// This indicates whether we're responsible for deleting the
		// JobAd. Most of the time, we aren't. But in at least one case,
		// we are.
	bool m_deleteJobAd;

		/** This is the identifier for this UserProc.  It's used for
			dprintf messages() and in some cases as a prefix for
			ClassAd attribute names.  For regular job procs, it's left
			as NULL, but for PRE/POST ScriptProc objects, it's got a
			real value...
		*/
	char* name;

	int soft_kill_sig = SIGTERM;
	int rm_kill_sig   = SIGTERM;
	int hold_kill_sig = SIGTERM;

	struct timeval job_start_time;
	struct timeval job_exit_time;

	virtual int outputOpenFlags() { return O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | O_LARGEFILE; }
	virtual int streamingOpenFlags( bool isOutput ) { return isOutput ? O_CREAT | O_TRUNC | O_WRONLY : O_RDONLY; }

	enum std_file_type {
		SFT_IN, SFT_OUT, SFT_ERR
	};

	bool getStdFile( std_file_type type,
	                 const char* attr,
	                 bool allow_dash,
	                 const char* log_header,
	                 int* out_fd,
	                 std::string & out_name);

	int openStdFile( std_file_type type,
	                 const char* attr,
	                 bool allow_dash,
	                 const char* log_header);

	void SetStdFiles(const int std_fds[], char const *std_fnames[]);

	virtual bool ThisProcRunsAlongsideMainProc();

	virtual char const *getArgv0();

private:

	void initKillSigs( void );

		// The following "pre defined" std fds and names are used
		// to influence openStdFile() and getStdFile() to force
		// them to use the specified fds or filenames in place of
		// calling jic.  The fds stored here are not closed by
		// UserProc.
	int m_pre_defined_std_fds[3];            // -1 if not set
	char const *m_pre_defined_std_fnames[3]; // NULL if not defined
	std::string m_pre_defined_std_fname_buf[3];
};

#endif /* _CONDOR_USER_PROC_H */
