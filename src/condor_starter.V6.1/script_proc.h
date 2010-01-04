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

#if !defined(_CONDOR_SCRIPT_PROC_H)
#define _CONDOR_SCRIPT_PROC_H

#include "condor_daemon_core.h"
#include "condor_classad.h"
#include "user_proc.h"

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
