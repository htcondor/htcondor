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

#if !defined(_CONDOR_STARTER_H)
#define _CONDOR_STARTER_H

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "list.h"
#include "os_proc.h"
#include "user_proc.h"

void set_resource_limits();
extern ReliSock *syscall_sock;

class CStarter : public Service
{
public:
	CStarter();
	virtual ~CStarter();

	virtual void Init(char peer[]);
	virtual void Config();
	virtual int ShutdownGraceful(int);	// vacate job(s)
	virtual int ShutdownFast(int);		// kill job(s) immediately
	virtual void StartJob();			// request job info and start job
	virtual int Suspend(int);
	virtual int Continue(int);
	virtual int PeriodicCkpt(int);
	virtual int Reaper(int pid, int exit_status);

	const char *GetWorkingDir() const { return WorkingDir; }

protected:
	List<UserProc> JobList;

private:
	char *InitiatingHost;
	char *Execute;
	char *UIDDomain;
	char *FSDomain;
	char WorkingDir[_POSIX_PATH_MAX];
	int Key;
	int ShuttingDown;
};

#endif