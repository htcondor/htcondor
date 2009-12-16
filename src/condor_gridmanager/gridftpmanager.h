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


#ifndef __GRIDFTPMANAGER_H__
#define __GRIDFTPMANAGER_H__


#include "gridmanager.h"
#include "proxymanager.h"

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
	static void UpdateLeases();

	static HashTable <HashKey, GridftpServer *> m_serversByProxy;
	static bool m_initialScanDone;
	static int m_updateLeasesTid;
	static bool m_configRead;
	static char *m_configUrlBase;

	void CheckServerSoon( int delta = 0 );
	void CheckServer();
	bool SubmitServerJob();
	bool ReadUrlBase();
	void CheckJobStatus();
	void CheckProxy();
	bool CheckPortError();
	bool FetchJobFiles();
	bool RemoveJob();
	void SetJobStatus( int new_status );

	int m_checkServerTid;
	Proxy *m_proxy;
	char *m_urlBase;
	char *m_requestedUrlBase;
	bool m_canRequestUrlBase;
	int m_refCount;
	int m_proxyExpiration;
	int m_lastProxyUpdateAttempt;
	SimpleList<int> m_registeredClients;
	PROC_ID m_jobId;
	MyString m_errorMessage;
	int m_lastSubmitAttempt;
	bool m_submitNow;
	int m_lastJobPoll;
	bool m_pollJobNow;
	int m_jobCondition;
	char *m_outputFile;
};

#endif
