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

#if !defined(_CONDOR_OS_PROC_H)
#define _CONDOR_OS_PROC_H

#include "user_proc.h"
#include "basename.h"

/** This is a generic sort of "OS" process, the base for other types
	of jobs.

*/
class OsProc : public UserProc
{
public:
		/// Constructor
	OsProc( ClassAd* jobAd );

		/// Destructor
	virtual ~OsProc();

		/** Here we do things like set_user_ids(), get the executable, 
			get the args, env, cwd from the classad, open the input,
			output, error files for re-direction, and (finally) call
			daemonCore->Create_Process().
		 */
	virtual int StartJob();

		/** In this function, we determine if pid == our pid, and if so
			do a CONDOR_job_exit remote syscall.  
			@param pid The pid that exited.
			@param status Its status
		    @return 1 if our OsProc is no longer active, 0 if it is
		*/
	virtual int JobCleanup( int pid, int status );

		/** In this function, we determine what protocol to use to
			send the shadow a CONDOR_job_exit remote syscall, which
			will cause the job to leave the queue and the shadow to
			exit.  We can't send this until we're all done transfering
			files and cleaning up everything. 
		*/
	virtual bool JobExit( void );

		/** Publish all attributes we care about for updating the job
			controller into the given ClassAd.  This function is just
			virtual, not pure virtual, since OsProc and any derived
			classes should implement a version of this that publishes
			any info contained in each class, and each derived version
			should also call it's parent's version, too.
			@param ad pointer to the classad to publish into
			@return true if success, false if failure
		*/
	virtual bool PublishUpdateAd( ClassAd* ad );

		/// Send a SIGSTOP
	virtual void Suspend();

		/// Send a SIGCONT
	virtual void Continue();

		/// Send a SIGTERM
	virtual bool ShutdownGraceful();

		/// Send a SIGKILL
	virtual bool ShutdownFast();

		/// Evict for condor_rm (ATTR_REMOVE_KILL_SIG)
	virtual bool Remove();

		/// Evict for condor_hold (ATTR_HOLD_KILL_SIG)
	virtual bool Hold();

		/// rename a core file created by this process
	void checkCoreFile( void );
	bool renameCoreFile( const char* old_name, const char* new_name );

protected:

	bool is_suspended;
	
		/// Number of pids under this OsProc
	int num_pids;

	bool dumped_core;

};

#endif
