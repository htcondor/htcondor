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

#include "..\condor_daemon_core.V6\condor_daemon_core.h"

class ClassAd;


class UserProc : public Service
{
public:
	UserProc() : JobAd(NULL), JobPid(-1), Cluster(-1), Proc(-1),
		Requested_Exit(0) {};
	// virtual ~UserProc();

		// StartJob: returns 1 on success, 0 on failure
		//   Starter deletes object if StartJob returns 0
	virtual int StartJob() = 0;
		// JobExit: returns 1 if exit handled, 0 if pid doesn't match
		//   Starter deletes object if JobExit returns 1
	virtual int JobExit(int pid, int status) = 0;

	virtual void Suspend() = 0;
	virtual void Continue() = 0;
	virtual void ShutdownGraceful() = 0;	// a.k.a. soft kill
	virtual void ShutdownFast() = 0;		// a.k.a. hard kill
	virtual void Ckpt() {};

	int GetJobPid() { return JobPid; }

protected:
	ClassAd *JobAd;
	int JobPid;
	int Cluster;
	int Proc;
	int Requested_Exit;
};

#endif