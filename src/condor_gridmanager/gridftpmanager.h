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

#ifndef __GRIDFTPMANAGER_H__
#define __GRIDFTPMANAGER_H__


#include "gridmanager.h"
#include "proxymanager.h"
#include "gt4job.h"

class GT4Job;

class GridftpServer : public Service
{
 protected:
	GridftpServer( Proxy *proxy );
	~GridftpServer();

 public:
	static GridftpServer *FindOrCreateServer( Proxy *proxy );
	static void Reconfig();

	void RegisterClient( int notify_tid, const char *req_url_base = NULL );
	void UnregisterClient( int notify_tid );
	bool IsEmpty();
	const char *GetUrlBase();
	const char *GetErrorMessage();
	bool UseSelfCred();

 protected:
	static bool ScanSchedd();
	static int UpdateLeases();

	static HashTable <HashKey, GridftpServer *> m_serversByProxy;
	static bool m_initialScanDone;
	static int m_updateLeasesTid;
	static bool m_configRead;
	static char *m_configUrlBase;

	void CheckServerSoon( int delta = 0 );
	int CheckServer();
	bool SubmitServerJob();
	bool ReadUrlBase();
	int CheckJobStatus();
	void CheckProxy();
	bool CheckPortError();
	bool RemoveJob();

	int m_checkServerTid;
	Proxy *m_proxy;
	char *m_urlBase;
	char *m_requestedUrlBase;
	bool m_canRequestUrlBase;
	int m_refCount;
	char *m_userLog;
	char *m_outputFile;
	char *m_errorFile;
	char *m_proxyFile;
	int m_proxyExpiration;
	SimpleList<int> m_registeredClients;
	PROC_ID m_jobId;
	MyString m_errorMessage;
	int m_lastSubmitAttempt;
};

#endif
