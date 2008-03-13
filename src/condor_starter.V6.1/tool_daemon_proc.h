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


#if !defined(_CONDOR_DAEMON_PROC_H)
#define _CONDOR_DAEMON_PROC_H

#include "user_proc.h"

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
		    @return True if our ToolDaemonProc is no longer active, else false.
		*/
	virtual bool JobReaper(int pid, int status);

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

		/// Evict for condor_rm (ATTR_REMOVE_KILL_SIG)
	virtual bool Remove();

		/// Evict for condor_hold (ATTR_HOLD_KILL_SIG)
	virtual bool Hold();


protected:

	bool job_suspended;

private:

	int ApplicationPid;
};


#endif /* _CONDOR_DAEMON_PROC_H */
