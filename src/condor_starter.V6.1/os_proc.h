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


class OsProc : public UserProc
{
public:
	OsProc();
	virtual ~OsProc();

		// StartJob: returns 1 on success, 0 on failure
		//   Starter deletes object if StartJob returns 0
	virtual int StartJob();
		// JobExit: returns 1 if exit handled, 0 if pid doesn't match
		//   Starter deletes object if JobExit returns 1
	virtual int JobExit(int pid, int status);

	virtual void Suspend();
	virtual void Continue();
	virtual void ShutdownGraceful();	// a.k.a. soft kill
	virtual void ShutdownFast();		// a.k.a. hard kill
protected:
	int job_suspended;
};

#endif