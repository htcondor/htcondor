/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
