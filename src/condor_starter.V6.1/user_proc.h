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
#if !defined(_CONDOR_USER_PROC_H)
#define _CONDOR_USER_PROC_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_distribution.h"
#include "utc_time.h"
#include "env.h"

class ClassAd;

/** This class is a base class for the various types of startable
	processes.  It defines a bunch of pure virtual functions that 
	are to be implemented in child classes.

 */
class UserProc : public Service
{
public:
		/// Constructor
	UserProc() : JobAd(NULL), name(NULL) { initialize(); }; 

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
			return 1, the starter will consider this UserProc done,
			remove it from the active job list, and put it in a list
			of jobs that are already cleaned up.
		    @return 1 if our UserProc is no longer active, 0 if it is
		*/
	virtual int JobCleanup(int pid, int status) = 0;

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
	virtual void Ckpt() {};

		/** Returns the pid of this job.
			@return The pid. */
	int GetJobPid() { return JobPid; }

		/** Check if user's job process has actually been started yet.
			For instance, it may not have been forked yet because we're
			waiting for data files to be transfered.
			@return true if job has been started, false if not. */
	bool JobStarted() { return JobPid > 0; }

		/** Was this job requested to exit by the starter, or did it 
			exit on its own?
		*/
	bool RequestedExit( void ) { return requested_exit; };

protected:

	void initialize( void );

	ClassAd *JobAd;
	int JobPid;
	int job_universe;
	int exit_status;
	bool requested_exit;

		/** This is the identifier for this UserProc.  It's used for
			dprintf messages() and in some cases as a prefix for
			ClassAd attribute names.  For regular job procs, it's left
			as NULL, but for PRE/POST ScriptProc objects, it's got a
			real value...
		*/
	char* name;

	int soft_kill_sig;
	int rm_kill_sig;
	int hold_kill_sig;

	UtcTime job_start_time;
	UtcTime job_exit_time;

	enum std_file_type {
		SFT_IN, SFT_OUT, SFT_ERR
	};

	int openStdFile( std_file_type type, const char* attr, 
					 bool allow_dash, bool &used_starter_fd,
					 const char* log_header );

private:

	void initKillSigs( void );

};

#endif /* _CONDOR_USER_PROC_H */
