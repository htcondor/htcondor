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
#if !defined(_CONDOR_USER_PROC_H)
#define _CONDOR_USER_PROC_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"

class ClassAd;

/** This class is a base class for the various types of startable
	processes.  It defines a bunch of pure virtual functions that 
	are to be implemented in child classes.

 */
class UserProc : public Service
{
public:
		/// Constructor
	UserProc() : JobAd(NULL), JobPid(-1), Cluster(-1), Proc(-1),
		Requested_Exit(0) {};

		/// Destructor
	virtual ~UserProc() {};

		/** Pure virtual functions: */
			//@{

		/** Start this job.  Starter should delete this object if 
			StartJob returns 0.
			@return 1 on success, 0 on failure.
		*/
	virtual int StartJob() = 0;

		/** Job exits. Starter deletes object if JobExit returns 1.
		    @return 1 if exit handled, 0 if pid doesn't match
		*/ 
	virtual int JobExit(int pid, int status) = 0;

		/** Publish all attributes we care about for updating the
			shadow into the given ClassAd.
			@param ad pointer to the classad to publish into
			@return true if success, false if failure
		*/
	virtual bool PublishUpdateAd( ClassAd* ad ) = 0;

		/** Suspend. */
	virtual void Suspend() = 0;

		/** Continue. */
	virtual void Continue() = 0;

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

protected:
	ClassAd *JobAd;
	int JobPid;
	int Cluster;
	int Proc;
	int Requested_Exit;
};

#endif
