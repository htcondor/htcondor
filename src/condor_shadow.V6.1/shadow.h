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

#if !defined(_SHADOW_H)
#define _SHADOW_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_classad.h"
#include "user_log.c++.h"
#include "exit.h"

const int SHADOW_SOCK_TIMEOUT = 90;		// timeout sockets after 90 seconds

class CShadow : public Service
{
public:
	CShadow();
	virtual ~CShadow();

	virtual void Init(char schedd_addr[], char host[], char capability[],
					  char cluster[], char proc[]);
	virtual void Config();
	virtual void RequestResource();
	virtual void Shutdown(int reason);
	virtual void InitUserLog();
	virtual int HandleSyscalls(Stream *sock);
	virtual int HandleJobRemoval(int sig);
	virtual void KillStarter();
	virtual void SetExitStatus(int status) { ExitStatus = status; }
	virtual void SetExitReason(int reason) { if (ExitReason != JOB_KILLED) ExitReason = reason; }
	virtual ClassAd *GetJobAd() { return JobAd; }
	virtual int GetCluster() { return Cluster; }
	virtual int GetProc() { return Proc; }
	virtual char *GetSpool() { return Spool; }
	virtual FILE* EmailUser(char *subjectline);
	virtual void dprintf_va( int flags, char* fmt, va_list args );
	virtual void dprintf( int flags, char* fmt, ... );

private:
	// config file parameters
	char *Spool;
	char *FSDomain;
	char *UIDDomain;
	char *CkptServerHost;
	bool UseAFS;
	bool UseNFS;
	bool UseCkptServer;

	// job parameters
	ClassAd *JobAd;
	int Cluster;
	int Proc;
	char Owner[_POSIX_PATH_MAX];
	char Iwd[_POSIX_PATH_MAX];
	char *ScheddAddr;
	bool JobExitedGracefully;
	int ExitStatus;
	int ExitReason;

	// claim parameters
	char *ExecutingHost;
	char *Capability;

	// others
	char *AFSCell;
	int StarterNum;
	UserLog ULog;
	ReliSock ClaimSock;
};

#endif