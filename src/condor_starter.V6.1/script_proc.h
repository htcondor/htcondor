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
#if !defined(_CONDOR_SCRIPT_PROC_H)
#define _CONDOR_SCRIPT_PROC_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "user_proc.h"

class ClassAd;

/** This class is for job wrapper scripts (pre/post) that the starter
	might have to spawn.
 */
class ScriptProc : public UserProc
{
public:
		/// Constructor
	ScriptProc( ClassAd* job_ad, const char* proc_name );

		/// Destructor
	virtual ~ScriptProc();

		/** Start this script.  Starter should delete this object if 
			StartJob returns 0.
			@return 1 on success, 0 on failure.
		*/
	virtual int StartJob();

		/** A pid exited.  If this ScriptProc wants to do any cleanup
			now that this pid has exited, it does so here.  If we
			return 1, the starter will consider this ScriptProc done,
			remove it from the active job list, and put it in a list
			of jobs that are already cleaned up.
		    @return 1 if our ScriptProc is no longer active, 0 if it is
		*/
	virtual int JobCleanup( int pid, int status );

		/** Job exits.  Starter has decided it's done with everything
			it needs to do, and we can now notify the job's controller
			we've exited so it can do whatever it wants to.
		    @return true on success, false on failure
		*/
	virtual bool JobExit( void );

		/** Publish all attributes we care about for updating the
			job controller into the given ClassAd.
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

protected:
	bool is_suspended;

};

#endif /* _CONDOR_SCRIPT_PROC_H */
