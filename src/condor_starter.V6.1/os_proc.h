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

#if !defined(_CONDOR_OS_PROC_H)
#define _CONDOR_OS_PROC_H

#include "user_proc.h"

/** This is a generic sort of "OS" process, the base for other types
	of jobs.

*/
class OsProc : public UserProc
{
public:
		/// Constructor
	OsProc();

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
			@return 1 if pid matches, 0 otherwise
		*/
	virtual int JobExit(int pid, int status);

		/** Publish all attributes we care about for updating the
			shadow into the given ClassAd.  This function is just
			virtual, not pure virtual, since OsProc and any derived
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

	// flag to TRUE is job suspended, else FALSE
	int job_suspended;

	// sinfull string of our shadow
	char ShadowAddr[35];

};

#endif
