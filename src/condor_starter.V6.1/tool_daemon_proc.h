/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#if !defined(_CONDOR_DAEMON_PROC_H)
#define _CONDOR_DAEMON_PROC_H

#include "user_proc.h"
#include "killfamily.h"

/** This is a "Tool" process in the TDP or "Tool Daemon Protocol"
	world.  This implements the Condor side of the TDP for spawning a
	tool that runs next to an application that might want to do
	profiling, debugging, etc.  It is derived directly from UserProc,
	since we don't want most of the functionality in OsProc and its
	children.
*/

class ToolDaemonProc : public UserProc
{
public:
		/// Constructor
	ToolDaemonProc( ClassAd *jobAd, int application_pid );

		/// Destructor
	virtual ~ToolDaemonProc();

		/** Here we do things like set_user_ids(), get the executable, 
			get the args, env, cwd from the classad, open the input,
			output, error files for re-direction, and (finally) call
			daemonCore->Create_Process().
		 */
	virtual int StartJob();

		/** In this function, we determine if pid == our pid, and if so
		    do whatever cleanup we need to now that the tool
		    has exited.  Also, this function will get called
		    whenever the application exits, so if the
		    ToolDaemon wants to do anything special when the
		    application is gone, we can go that here, too.
		    @param pid The pid that exited.
		    @param status Its status
		    @return 1 if our ToolDaemonProc is no longer active, 0 if it is
		*/
	virtual int JobCleanup(int pid, int status);

	/** ToolDaemonProcs don't need to send a remote system call to
	    the shadow to tell it we've exited.  So, we want our own
	    version of this function that just returns without doing
	    anything. 
	*/
	virtual bool JobExit( void );

		/** Publish all attributes we care about for updating the
			shadow into the given ClassAd.  This function is just
			virtual, not pure virtual, since ToolDaemonProc and any derived
			classes should implement a version of this that publishes
			any info contained in each class, and each derived version
			should also call it's parent's version, too.
			@param ad pointer to the classad to publish into
			@return true if success, false if failure
		*/
	virtual bool PublishUpdateAd( ClassAd* ad );

		/// Send a DC_SIGSTOP
	virtual void Suspend();

		/// Send a DC_SIGCONT
	virtual void Continue();

		/// Send a DC_SIGTERM
	virtual bool ShutdownGraceful();

		/// Send a DC_SIGKILL
	virtual bool ShutdownFast();

protected:

	bool job_suspended;

private:

	int ApplicationPid;

	ProcFamily *family;

	// timer id for periodically taking a ProcFamily snapshot
	int snapshot_tid;
};


#endif /* _CONDOR_DAEMON_PROC_H */
