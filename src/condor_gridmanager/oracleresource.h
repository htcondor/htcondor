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


#ifndef ORACLERESOURCE_H
#define ORACLERESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "oci.h"

#include "oraclejob.h"


class OracleJob;
class OciSession;
class OciServer;

OciSession *GetOciSession( const char *db_name, const char *db_username,
						   const char *db_password );

class OciSession : public Service
{
 public:
	OciSession( OciServer *oci_server, const char *db_username,
				const char *db_password );
	~OciSession();

	int Initialize();

	void ServerConnectionClosing();

	void RegisterJob( OracleJob *job );
	void UnregisterJob( OracleJob *job );

	int AcquireSession( OracleJob *job, OCISvcCtx *&svc_hndl,
						OCIError *&err_hndl );
	int ReleaseSession( OracleJob *job );

	void RequestProbeJobs();

 private:
	int OpenSession( OCIError *&err_hndl );
	int CloseSession();

	int doProbeJobs();

	bool initDone;
	OciServer *server;
	char *username;
	char *password;
	OCISvcCtx *ociSvcCtxHndl;
	OCISession *ociSessionHndl;
	OCITrans *ociTransHndl;
	OCIError *ociErrorHndl;
		// We use a pointer to a List here because we want to put
		// OciSessions into Lists, which require operator=() to be defined,
		// but List itself doesn't have a usable operator=().
	List<OracleJob> *registeredJobs;
	bool sessionOpen;
	int lastProbeJobsTime;
	int probeJobsTid;
};

class OciServer : public Service
{
 public:
	OciServer( const char *db_name );
	~OciServer();

	int Initialize();

	void RegisterSession( OciSession *session, const char *username );
	void UnregisterSession( OciSession *session, const char *username );
	OciSession *FindSession( const char *username );

	int SessionActive( OciSession *session, OCIServer *&svr_hndl,
					   OCIError *&err_hndl );
	void SessionInactive( OciSession *session );

 private:
	int IdleDisconnectHandler();

	int ServerConnect();
	int ServerDisconnect();

	bool initDone;
	char *dbName;
	HashTable<HashKey, OciSession *> sessionsByUsername;
	List<OciSession> *activeSessions;
	OCIServer *ociServerHndl;
	OCIError *ociErrorHndl;
	bool connectionOpen;
	int idleDisconnectTid;
};

#endif
